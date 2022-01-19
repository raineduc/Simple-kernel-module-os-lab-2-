#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  bool has_pid = false;
  bool has_vid = false;
  bool has_devid = false;
  int pid;
  unsigned int vendor_id;
  unsigned int device_id; 

  for (int i = 0; i < argc - 1; i++) {
    if (sscanf(argv[1 + i], "--pid=%d", &pid) == 1) has_pid = true;
    if (sscanf(argv[1 + i], "--vid=%x", &vendor_id) == 1) has_vid = true;
    if (sscanf(argv[1 + i], "--devid=%x", &device_id)) has_devid = true;
  }

  FILE *file = fopen("/sys/kernel/debug/labmod/labmod_io", "r+");
	if (file == NULL) {
		printf("Can not open file\n");
		exit(1);
	}
  clearerr(file);

  if (has_pid) {
    char *buffer[BUFFER_SIZE];
    fprintf(file, "pid: %d", pid);
    while (true) {
      char *msg = fgets(buffer, BUFFER_SIZE, file);
      if (msg == NULL) {
        if (feof(file)) break;
        fprintf(stderr, "Page struct reading failed with errno code: %d\n", errno);
      } else {
        printf(msg);
      }
    }
  }

  if (has_vid && has_devid) {
    char *buffer[BUFFER_SIZE];
    fprintf(file, "vid: %x, devid: %x", vendor_id, device_id);
    while (true) {
      char *msg = fgets(buffer, BUFFER_SIZE, file);
      if (msg == NULL) {
        if (feof(file)) break;
        fprintf(stderr, "Pci_dev struct reading failed with errno code: %d\n", errno);
      } else {
        printf(msg);
      }
    }
  }

  
  if (!has_vid && !has_devid && !has_pid) {
    printf("No params provided\n");
  }

	fclose(file);
	return 0;
}