#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/pci.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#define BUFFER_SIZE 4096


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rain");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("1.0");

static const char *NONE_STRUCT_MESG = "No structs recorded to file\n";

struct msg_to_kernel {
  int pid;
  int pci_vendor_id;
  int pci_device_id;
};

enum struct_type { NONE, PAGE, DEV };

static struct dentry *root_dir;
static struct dentry *args_file;

static enum struct_type current_struct = NONE; 
static struct page *page_struct = NULL;
static struct pci_dev *pci_dev_struct = NULL;

struct msg_page {
  unsigned long flags;
  int refcount;
};

struct msg_pci_dev {
  unsigned short vendor;
  unsigned short device;
  unsigned char pin;
  unsigned char revision;
  unsigned int fn;
};

struct msg_to_user {
  struct msg_page page;
  struct msg_pci_dev pci_dev;
};





//func declarations
static void fill_structs(int pid, unsigned int vendor_id, unsigned int device_id);
static struct msg_to_user *build_msg_to_user(void);
struct page *mod_get_page(int pid);
struct pci_dev *mod_get_pci_dev(unsigned int vendor_id, unsigned int device_id);
static void free_msg_to_user(const struct msg_to_user *msg);
static void print_page(struct seq_file *file, struct page *page);
static void print_dev(struct seq_file *file, struct pci_dev *pci_dev);
static int print_struct(struct seq_file *file, void *data);





// file operations
static int mod_open(struct inode *inode, struct file *file) {
    pr_info("labmod: debugfs file opened\n");
    return single_open(file, print_struct, NULL);
}

// static ssize_t mod_read(struct file *file, char __user *buffer, size_t length, loff_t *ptr_offset) {
//   printk(KERN_INFO "labmod: result\n");

//   char kernel_message[BUFFER_SIZE];

//   if (current_struct == PAGE) {
//     print_page(kernel_message, page_struct);
//   } else if (current_struct == DEV) {
//     print_dev(kernel_message, pci_dev_struct);
//   } else {
//     sprintf(kernel_message, NONE_STRUCT_MESG);
//   }

//   if (copy_to_user(buffer, kernel_message, strlen(kernel_message) + 1)) {
//     printk(KERN_ERR "labmod: Can't send to user");
//     return -EFAULT;
//   }

//   printk(KERN_INFO "labmod: Structs sent to user");
//   printk(KERN_INFO "labmod: offet %lld", *ptr_offset);
//   printk(KERN_INFO "labmod: size %zu", length);
//   return (ptr_offset != NULL && *ptr_offset > 0) ? 0 : length;
// }

static ssize_t mod_write(struct file *file, const char __user *buffer, size_t length, loff_t *ptr_offset) {
  printk(KERN_INFO "labmod: get arguments\n");
  char user_message[BUFFER_SIZE];
  int pid;
  unsigned int vendor_id, device_id;

  if (copy_from_user(user_message, buffer, length)) {
    printk(KERN_ERR "labmod: Can't write to kernel");
    return -EFAULT;
  }

  if (sscanf(user_message, "pid: %d",&pid) == 1) {
    printk(KERN_INFO "labmod: pid: %d", pid);
    current_struct = PAGE;
    page_struct = mod_get_page(pid);
  } else if (sscanf(user_message, "vid: %x, devid: %x", &vendor_id, &device_id) == 2) {
    printk(KERN_INFO "labmod: pci vendor id %x, pci device id %x", vendor_id, device_id);
    current_struct = DEV;
    pci_dev_struct = mod_get_pci_dev(vendor_id, device_id);
  } else {
    printk(KERN_ERR "labmod: Can't parse input");
    return -EINVAL;
  }
  single_open(file, print_struct, NULL);
  return strlen(user_message);
}

static int mod_release(struct inode *inode, struct file *file) {
    pr_info("labmod: debugfs file released\n");
    return 0;
}

static struct file_operations mod_io_ops = {
  .owner = THIS_MODULE,
  .read = seq_read,
  .write = mod_write,
  .open = mod_open,
  .release = mod_release
};

static struct msg_to_user *build_msg_to_user(void) {
  if (page_struct == NULL || pci_dev_struct == NULL) return NULL;
  struct msg_to_user *msg_to_user = (struct msg_to_user*) kmalloc(sizeof(struct msg_to_user), GFP_KERNEL);
  msg_to_user->page.flags = page_struct->flags;
  msg_to_user->page.refcount = page_struct->_refcount.counter;

  msg_to_user->pci_dev.vendor = pci_dev_struct->vendor;
  msg_to_user->pci_dev.device = pci_dev_struct->device;
  msg_to_user->pci_dev.pin = pci_dev_struct->pin;
  msg_to_user->pci_dev.revision = pci_dev_struct->revision;
  msg_to_user->pci_dev.fn = PCI_FUNC(pci_dev_struct->devfn);;

  return msg_to_user;
}

static void free_msg_to_user(const struct msg_to_user *msg) {
  if (msg != NULL) kfree(msg);
}





// module entry/exit points
static int __init mod_init(void) {
  printk(KERN_INFO "labmod: module loaded\n");

  root_dir = debugfs_create_dir("labmod", NULL);
  args_file = debugfs_create_file("labmod_io", 0777, root_dir, NULL, &mod_io_ops);

  return 0;
}

static void __exit mod_exit(void) {
  debugfs_remove_recursive(root_dir);
  printk(KERN_INFO "labmod: module unloaded\n");
}





//structs output

static int print_struct(struct seq_file *file, void *data) {
  if (current_struct == PAGE) {
    if (page_struct == NULL) {
      seq_printf(file, "Page struct with provided id not found\n");
      return 0;
    }
    print_page(file, page_struct);
  } else if (current_struct == DEV) {
    if (pci_dev_struct == NULL) {
      seq_printf(file, "Pci_dev struct with provided params not found\n");
      return 0;
    }
    print_dev(file, pci_dev_struct);
  } else {
    seq_printf(file, NONE_STRUCT_MESG);
  }
  return 0;
}

static void print_page(struct seq_file *file, struct page *page) {
  seq_printf(file, "page structure: {\n");
  seq_printf(file, "  flags: %lu,\n", page->flags);
  seq_printf(file, "  refcount: %d\n", page->_refcount.counter);
  seq_printf(file, "}\n");
}

static void print_dev(struct seq_file *file, struct pci_dev *pci_dev) {
  seq_printf(file, "dev structure: {\n"); 
  seq_printf(file, "  vendor ID: %u,\n", pci_dev->vendor);
  seq_printf(file, "  device ID: %u\n", pci_dev->device);
  seq_printf(file, "  interrupt pin: %u\n", pci_dev->pin);
  seq_printf(file, "  PCI revision: %u\n", pci_dev->revision);
  seq_printf(file, "  Function index: %u\n", PCI_FUNC(pci_dev->devfn));
  seq_printf(file, "}\n");
}




//extracting structs
static void fill_structs(int pid, unsigned int vendor_id, unsigned int device_id) {
  page_struct = mod_get_page(pid);
  pci_dev_struct = mod_get_pci_dev(vendor_id, device_id);
}

static struct page *get_process_page(struct mm_struct* mm, long address) {
    pgd_t *pgd;
    p4d_t* p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct page *page = NULL;

    // five-level page tables

    // Page Global Directory level
    pgd = pgd_offset(mm, address);
    if (!pgd_present(*pgd)) {
        return NULL;
    }

    // P4D level
    p4d = p4d_offset(pgd, address);
    if (!p4d_present(*p4d)) {
        return NULL;
    }

    //Page Upper Directory level
    pud = pud_offset(p4d, address);
    if (!pud_present(*pud)) {
        return NULL;
    }

    // Page Middle Directory level
    pmd = pmd_offset(pud, address);
    if (!pmd_present(*pmd)) {
        return NULL;
    }

    // Page Table Entry level
    pte = pte_offset_kernel(pmd, address);
    if (!pte_present(*pte)) {
        return NULL;
    }

    // finally, the page itself
    page = pte_page(*pte);
    return page;
}

struct page *mod_get_page(int pid) {
  struct task_struct *task;
  struct mm_struct *mm;
  struct vm_area_struct *vm_current;
  struct page *page_struct;

  task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
  if (task == NULL) {
    printk(KERN_ERR "labmod: task is not found\n");
    return NULL;
  }
  mm = task->mm;
  if (mm == NULL) {
    printk(KERN_ERR "kmod: process has no address space\n");
    return NULL;
  }

  vm_current = mm->mmap;
  long start = vm_current->vm_start;
  long end = vm_current->vm_start;
  long address = start;

  while (address <= end) {
    page_struct = get_process_page(mm, address);
    address += PAGE_SIZE;
    if (page_struct != NULL) {
      break;
    }
  }

  if (page_struct == NULL) {
    printk(KERN_INFO "kmod: error occured while getting page\n");
  }

  return page_struct;
}

struct pci_dev *mod_get_pci_dev(unsigned int vendor_id, unsigned int device_id) {
  struct pci_dev *dev = pci_get_device(vendor_id, device_id, NULL);
  if (dev == NULL) {
    printk(KERN_INFO "labmod: device wasn't found");
  }

  return dev;
}





module_init(mod_init);
module_exit(mod_exit);