// Ryan Laurents
// 1000763099
// Homework 4 (FAT32)
//
// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>


#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

struct DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];

uint16_t BPB_BytesPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATS;
uint32_t BPB_FATSz32;

// Global bool to say if a file is currently open
bool fileOpened = false;
// Global file pointer
FILE *file;

// Convert the logical block address to the offset (sector)
uint32_t LBAToOffset(uint32_t sector)
{
  return ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
}

// Return the next logical block
uint16_t NextLB(uint32_t sector)
{
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
  uint16_t val;
  fseek(file, FATAddress, SEEK_SET);
  fread(&val, 2, 1, file);
  return val;
}

int compare(char *userString, char *directoryString)
{
  char *dotdot = "..";

  if(strncmp(dotdot, userString, 2) == 0)
  {
    if(strncmp(userString, directoryString, 2) == 0)
    {
        return 1;
    } 
    return 0;
  }

  char IMG_Name[12];
  strncpy(IMG_Name, directoryString, 11);
  IMG_Name[11] = '\0';

  char input[11];
  memset(input, 0, 11);
  strncpy(input, userString, strlen(userString));

  char expanded_name[12];
  memset(expanded_name, ' ', 12);

  char *token = strtok(input, ".");
  strncpy(expanded_name, token, strlen(token));

  token = strtok(NULL, ".");

  if(token)
  {
    strncpy((char *)(expanded_name + 8), token, strlen(token));
  }

  expanded_name[11] = '\0';

  int i;
  for(i = 0; i < 11; i++)
  {
    expanded_name[i] = toupper(expanded_name[i]);
  }

  if(strncmp(expanded_name, IMG_Name, 11) == 0)
  {
    return 1;
  }
  return 0;
      
}
    
#define MAX_NUM_ARGUMENTS 4
#define NUM_ENTRIES 16

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

// Print out BPB information to the command line
int bpb()
{
  printf("BPB_BytesPerSec: %d 0x%x\n", BPB_BytesPerSec, BPB_BytesPerSec);
  printf("BPB_SecPerClus: %d 0x%x\n", BPB_SecPerClus, BPB_SecPerClus);
  printf("BPB_RsvdSecCnt: %d 0x%x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
  printf("BPB_NumFATS: %d 0x%x\n", BPB_NumFATS, BPB_NumFATS);
  printf("BPB_FATSz32: %d 0x%x\n", BPB_FATSz32, BPB_FATSz32);

  return 0;
}

// Same ls as linux, just with FAT32
int ls()
{
  int i;
  for(i = 0; i < NUM_ENTRIES; i++)
  {
    char filename[12];
    strncpy(filename, dir[i].DIR_Name, 11);
    filename[11] = '\0';

    if((dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY || dir[i].DIR_Attr == ATTR_ARCHIVE) && filename[0] != 0xffffffe5)
    {
      printf("%s\n", filename);
    }
  }
  return 0;
}

// Classic change directory to specified directory name
int cd(char *directoryName)
{
  int i;
  int found = 0;
  for(i = 0; i < NUM_ENTRIES; i++)
  {
    if(compare(directoryName, dir[i].DIR_Name))
    {
      int cluster = dir[i].DIR_FirstClusterLow;
      
      if(cluster == 0)
      {
        cluster = 2;
      }

      int offset = LBAToOffset(cluster);
      fseek(file, offset, SEEK_SET);
      fread(dir, sizeof(struct DirectoryEntry), NUM_ENTRIES, file);

      found = 1;
      break;
    }
  }
  if(!found)
  {
    printf("Error: Directory not found\n");
    return -1;
  }
  return 0;
}

// This function returns the Attr, size, and cluster of the specified file
int statFile(char *fileName)
{
  int i;
  int found = 0;
  for(i = 0; i < NUM_ENTRIES; i++)
  {
    if(compare(fileName, dir[i].DIR_Name))
    {
      printf("%s Attr: %d Size: %d Cluster: %d\n", fileName, dir[i].DIR_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
      found = 1;
    }
  }
  if(!found)
  {
    printf("Error: File Not Found\n");
  }
  return 0;
}

// getFile will grab the specified file and move it out to your working directory in bash. If there are two paramters, it will also change the filename.
int getFile(char *originalFilename, char *newFileName)
{
  FILE *origFile;

  if(newFileName == NULL)
  {
    origFile = fopen(originalFilename, "w");
    if(origFile == NULL)
    {
      printf("Could not open file %s\n", originalFilename);
      perror("Error: ");
    }
  }
  else
  {
    origFile = fopen(newFileName, "w");
    if(origFile == NULL)
    {
      printf("Could not open file %s\n", newFileName);
      perror("Error: ");
    }
  }

  int i;
  int found = 0;

  for(i = 0; i < NUM_ENTRIES; i++)
  {
    if(compare(originalFilename, dir[i].DIR_Name))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      found = 1;
      
      int bytesRemainingToRead = dir[i].DIR_FileSize;
      int offset = 0;
      unsigned char buffer[512];

      while(bytesRemainingToRead >= BPB_BytesPerSec)
      {
        offset = LBAToOffset(cluster);
        fseek(file, offset, SEEK_SET);
        fread(buffer, 1, BPB_BytesPerSec, file);
        fwrite(buffer, 1, 512, origFile);

        cluster = NextLB(cluster);

        bytesRemainingToRead = bytesRemainingToRead - BPB_BytesPerSec;
      }

      if(bytesRemainingToRead)
      {
        offset = LBAToOffset(cluster);
        fseek(file, offset, SEEK_SET);
        fread(buffer, 1, bytesRemainingToRead, file);
        fwrite(buffer, 1, bytesRemainingToRead, origFile);
      }

      fclose(origFile);
    }
  }
}

// readFile will read the specified file starting at the offset position and reading reqBytes number of bytes. This will print to the command line.
int readFile(char *filename, int reqOffset, int reqBytes)
{
  int i;
  int found = 0;
  int bytesRemainingToRead = reqBytes;

  if(reqOffset < 0)
  {
    printf("Error: Offset can not be less than 0\n");
  }

  for(i = 0; i < NUM_ENTRIES; i++)
  {
    if(compare(filename, dir[i].DIR_Name))
    {
      int cluster = dir[i].DIR_FirstClusterLow;
      found = 1;

      int searchSize = reqOffset;

      while(searchSize >= BPB_BytesPerSec)
      {
        cluster = NextLB(cluster);
        searchSize = searchSize - BPB_BytesPerSec;
      }

      int offset = LBAToOffset(cluster);
      int byteOffset = (reqOffset % BPB_BytesPerSec);
      fseek(file, offset + byteOffset, SEEK_SET);

      unsigned char buffer[BPB_BytesPerSec];

      int firstBlockBytes = BPB_BytesPerSec - reqOffset;
      fread(buffer, 1, firstBlockBytes, file);

      for(i = 0; i < firstBlockBytes; i++)
      {
        printf("%x ", buffer[i]);
      }

      bytesRemainingToRead = bytesRemainingToRead - firstBlockBytes;

      while(bytesRemainingToRead >= 512)
      {
        cluster = NextLB(cluster);
        offset = LBAToOffset(cluster);
        fseek(file, offset, SEEK_SET);
        fread(buffer, 1, BPB_BytesPerSec, file);

        for(i = 0; i < BPB_BytesPerSec; i++)
        {
          printf("%x ", buffer[i]);
        }

        bytesRemainingToRead = bytesRemainingToRead - BPB_BytesPerSec;
      }

      if(bytesRemainingToRead)
      {
        cluster = NextLB(cluster);
        offset = LBAToOffset(cluster);
        fseek(file, offset, SEEK_SET);
        fread(buffer, 1, bytesRemainingToRead, file);

        for(i = 0; i < bytesRemainingToRead; i++)
        {
          printf("%x ", buffer[i]);
        }
      }

      printf("\n");
    }
  }

  if(!found)
  {
    printf("Error: File Not found\n");
    return -1;
  }
  return 0;
}

int main()
{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );


  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    if(cmd_str[0] == '\n')
    {
      continue;
    }

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                               
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // If user enters "open", open the file for use.
    if(strcmp(token[0], "open") == 0)
    {
      file = fopen(token[1], "r");
      if(file == NULL)
      {
        printf("Error: File system image not found.\n");
      }
      else if(fileOpened)
      {
        printf("Error: File system image already open.\n");
      }
      else
      {
        // Find all the BPB stats for this file
        fileOpened = true;
        fseek(file, 11, SEEK_SET);
        fread(&BPB_BytesPerSec, 1, 2, file);

        fseek(file, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1, 1, file);

        fseek(file, 14, SEEK_SET);
        fread(&BPB_RsvdSecCnt, 1, 2, file);

        fseek(file, 16, SEEK_SET);
        fread(&BPB_NumFATS, 1, 1, file);

        fseek(file, 36, SEEK_SET);
        fread(&BPB_FATSz32, 4, 1, file);

        fseek(file, 0x100400, SEEK_SET);
        fread(dir, 16,sizeof(struct DirectoryEntry), file);
      }
    }

    // If command = "close" close the current file, set global bool fileOpened to false, and set file pointer to NULL
    else if(strcmp(token[0], "close") == 0)
    {
      if(fileOpened)
      {
        fclose(file);
        file = NULL;
        fileOpened = false;
      }
      else
      {
        printf("Error: File system not open.\n");
      }
    }

    // For remaining commands, if a command matches any of the below, call their associated functions defined above.
    else if(strcmp(token[0], "bpb") == 0)
    {
      if(fileOpened)
      {
        bpb();
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "ls") == 0)
    {
      if(fileOpened)
      {
        ls();
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "cd") == 0)
    {
      if(fileOpened)
      {
        cd(token[1]);
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "read") == 0)
    {
      if(fileOpened)
      {
        readFile(token[1], atoi(token[2]), atoi(token[3]));
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "get") == 0)
    {
      if(fileOpened)
      {
        getFile(token[1], token[2]);
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "stat") == 0)
    {
      if(fileOpened)
      {
        statFile(token[1]);
      }
      else
      {
        printf("Error: File Image Not Opened.\n");
      }
    }

    else if(strcmp(token[0], "quit") == 0)
    {
      exit(0);
    }

    else if(strcmp(token[0], "exit") == 0)
    {
      exit(0);
    }

    free( working_root );

  }
  return 0;
}
