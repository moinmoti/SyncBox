#include "stdlib.h"
#include "sys/types.h"
#include "sys/file.h"
#include "unistd.h"
#include "sys/stat.h"
#include "sys/dir.h"
#include "string.h"



int checkFile(char * fileName) {
  struct stat buffer;
  char permissions[10];

  // checking if the file exists.
  if (stat(fileName, &buffer) < 0) return 1;
  write(1, "Do File Exists :\tYes\n", strlen("Do File Exists :\tYes\n"));
  write(1, "File Permissions\t", strlen("File Permissions\t"));

  // extracting the permissions of the file and storing in a string
  strcpy(permissions, (S_ISDIR(buffer.st_mode)) ? "d" : "-");
  strcat(permissions, (buffer.st_mode & S_IRUSR) ? "r" : "-");
  strcat(permissions, (buffer.st_mode & S_IWUSR) ? "w" : "-");
  strcat(permissions, (buffer.st_mode & S_IXUSR) ? "x" : "-");
  strcat(permissions, (buffer.st_mode & S_IRGRP) ? "r" : "-");
  strcat(permissions, (buffer.st_mode & S_IWGRP) ? "w" : "-");
  strcat(permissions, (buffer.st_mode & S_IXGRP) ? "x" : "-");
  strcat(permissions, (buffer.st_mode & S_IROTH) ? "r" : "-");
  strcat(permissions, (buffer.st_mode & S_IWOTH) ? "w" : "-");
  strcat(permissions, (buffer.st_mode & S_IXOTH) ? "x" : "-");
  write(1, permissions, strlen(permissions));
  write(1, "\n", strlen("\n"));

  // checking if the permissions are set correctly
  if (strncmp(permissions, "d", 1) == 0) {

    // checking the file permissions for the directory
    if (strcmp(permissions, "drwx--x--x") != 0) {
      write(2, "Error : Directory Permissions are not set correctly\n", strlen("Error : Directory Permissions are not set correctly\n"));
      write(2, "The Directory Permissions should be :\tdrwx--x--x\n", strlen("The Directory Permissions should be :\tdrwx--x--x\n"));
    } else {
      write(1, "Directory Permissions are set correctly\n", strlen("Directory Permissions are set correctly\n"));
    }
  } else {

    // checking the file permissions for the file
    if (strcmp(permissions, "-rw-------") != 0) {
      write(2, "Error : File Permissions are not set correctly\n", strlen("Error : File Permissions are not set correctly\n"));
      write(2, "The File Permissions should be :\t-rw-------\n", strlen("The File Permissions should be :\t-rw-------\n"));
    } else {
      write(1, "File Permissions are set correctly\n", strlen("File Permissions are set correctly\n"));
    }
  }

  return 0;
}



int main(int argc, char const *argv[]) {
  int original_file, reverse_file;

  //checking for directory permissions and if the directroy even exists
  write(1, "Checking for Directory Assignment :\n", strlen("Checking for Directory Assignment :\n"));
  if (checkFile("Assignment")) {
    char msg0[] = "Error : No such directory exists\n";
    write(2, msg0, strlen(msg0));
    exit(-1);
  }

  // checking for file permissions and if the file even exists
  write(1, "Checking for File :\n", strlen("Checking for File :\n"));
  char dummy[] = "./Assignment/";
  strcat(dummy, argv[1]);
  if (checkFile(dummy)) {
    char msg1[] = "No such file exists\n";
    write(2, msg1, strlen(msg1));
    exit(-1);
  }

  long long int size, i;
  char buffer1, buffer2;

  // opening the original file
  original_file = open(argv[1], O_RDONLY, 0);

  // opening the which contains the reversed data
  reverse_file = open(dummy, O_RDONLY, 0);

  // measuring the size of the content of the file
  size = lseek(original_file, 0, 2);

  // loop for checking whether the file has been properly reversed
  for (i = 0; i < size-1; i++) {

    // reading bytes from the beginning of the original_file
    lseek(original_file, i, 0);
    read(original_file, &buffer1, 1);

    // reading bytes from the ending of the reversed file
    lseek(reverse_file, -(i+1), 2);
    read(reverse_file, &buffer2, 1);

    // checking if the characters match from both the files
    if (buffer1 != buffer2) {
      char msg2[] = "Error : File Content is not reversed properly\n";
      write(2, msg2, strlen(msg2));
      exit(-1);
    }
  }

  // if the loop ends successfully the file has been properly reversed
  char msg3[] = "File Content is reversed properly\n";
  write(1, msg3, strlen(msg3));

  // closing both the files
  close(original_file);
  close(reverse_file);

  return 0;
}

//Copyright Moin Hussain Moti (c) 2016 Copyright Holder All Rights Reserved.
