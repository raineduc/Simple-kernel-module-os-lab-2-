#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

struct msg_to_kernel {
	int pid;
	int pci_vendor_id;
	int pci_device_id;
};

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

int main(int argc, char *argv[]) {
  struct msg_to_kernel msg_to_kernel;
  struct msg_to_user *msg_to_user;
  
  if (argc < 4) {
    fprintf(stderr, "Not enough arguments, expected 4\n");
    exit(1);
  }

  msg_to_kernel = (struct msg_to_kernel) {
    .pid = strtol(argv[1], NULL, 10),
    .pci_vendor_id = strtol(argv[2], NULL, 16),  
    .pci_device_id = strtol(argv[3], NULL, 16)
  };

  FILE *file = fopen("/sys/kernel/debug/labmod/labmod_io", "r+");
	if (file == NULL) {
		printf("Can not open file\n");
		exit(1);
	}
  clearerr(file);

  fwrite(&msg_to_kernel, sizeof(struct msg_to_kernel), 1, file);
	if (ferror(file)) {
		fprintf(stderr, "Structs writing failed with errno code: %d\n", errno);
		exit(1);
	}

  fread(msg_to_user, sizeof(struct msg_to_user), 1, file);
  if (ferror(file)) {
    if (errno == EINVAL) {
      fprintf(stderr, "Corresponding structs not found\n");
    } else {
      fprintf(stderr, "Structs reading failed with errno code: %d\n", errno);
    } 
    exit(1);
  }

  if (msg_to_user == NULL) {
    fprintf(stderr, "Corresponding structs not found\n");
    exit(1);
  }

  printf("dev structure: {\n"); 
  printf("  vendor ID: %u,\n", msg_to_user->pci_dev.vendor);
  printf("  device ID: %u\n", msg_to_user->pci_dev.device);
  printf("  interrupt pin: %u\n", msg_to_user->pci_dev.pin);
  printf("  PCI revision: %u\n", msg_to_user->pci_dev.revision);
  printf("  Function index: %u\n", msg_to_user->pci_dev.fn);
  printf("}\n");
  printf("page structure: { flags: %lu, refcount: %d }\n", msg_to_user->page.flags, msg_to_user->page.refcount);
	fclose(file);
	return 0;
}