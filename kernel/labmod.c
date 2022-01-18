#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/pci.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rain");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("1.0");

struct msg_to_kernel {
  int pid;
  int pci_vendor_id;
  int pci_device_id;
};

static struct dentry *root_dir;
static struct dentry *args_file;

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





// file operations
static int mod_open(struct inode *inode, struct file *file) {
    pr_info("labmod: debugfs file opened\n");
    return 0;
}

static ssize_t mod_read(struct file *file, char __user *buffer, size_t length, loff_t *ptr_offset) {
  printk(KERN_INFO "labmod: result\n");

  struct msg_to_user *msg_to_user;
  msg_to_user = build_msg_to_user();

  if (msg_to_user == NULL) {
    printk(KERN_ERR "labmod: there's no recorded structs");
    return -EINVAL;
  }

  if (copy_to_user(buffer, msg_to_user, sizeof(struct msg_to_user))) {
    printk(KERN_ERR "labmod: Can't send to user");
    free_msg_to_user(msg_to_user);
    return -EFAULT;
  }

  printk(KERN_INFO "labmod: Structs sent to user");

  free_msg_to_user(msg_to_user);
  return length;
}

static ssize_t mod_write(struct file *file, const char __user *buffer, size_t length, loff_t *ptr_offset) {
  printk(KERN_INFO "labmod: get arguments\n");
  struct msg_to_kernel msg_to_kernel;

  if (copy_from_user(&msg_to_kernel, buffer, sizeof(struct msg_to_kernel))) {
    printk(KERN_ERR "labmod: Can't write to kernel");
    return -EFAULT;
  }

  printk(KERN_INFO "labmod: pid %d, pci vendor id %d, pci device id %d", msg_to_kernel.pid, msg_to_kernel.pci_vendor_id, msg_to_kernel.pci_device_id);

  fill_structs(msg_to_kernel.pid, msg_to_kernel.pci_vendor_id, msg_to_kernel.pci_device_id);

  return length;
}

static int mod_release(struct inode *inode, struct file *file) {
    pr_info("labmod: debugfs file released\n");
    return 0;
}

static struct file_operations mod_io_ops = {
  .owner = THIS_MODULE,
  .read = mod_read,
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