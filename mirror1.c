#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#define PORT 54321
#define INPUT_THREASHOLD 100
#define BUFFER_SIZE 2048
#define LENGTH_MAX 256
#define DIR_THRESHOLD_MAX 1000
#define TEMP_DUMP_TXT "temporary_file_list.txt"
#define ZIP_FILE_NAME "w24project/temp.tar.gz"
#define RESPONSE_TEXT 1
#define RESPONSE_ZIP 2

int connection_counter = 0;
// Function to send response to client based on type
int send_response(char * content, int type, int client_socket);
int send_response(char * content, int type, int client_socket) {
  // Send the response type indicator
  send(client_socket, & type, sizeof(type), 0);

  // Send the content based on the response type
  if (type == RESPONSE_TEXT) {
    send(client_socket, content, strlen(content), 0);
  } else if (type == RESPONSE_ZIP) {
    // Open the file for reading
    FILE * fp = fopen(content, "rb");
    if (fp == NULL) {
      perror("Error opening file for reading");
      return -1; // Indicate failure
    }

    // Read and send the file contents in chunks
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
      send(client_socket, buffer, bytes_read, 0);
    }

    // Close the file
    fclose(fp);
  } else {
    // Invalid response type
    fprintf(stderr, "Invalid response type\n");
    return -1; // Indicate failure
  }

  return 0; // Indicate success
}
// Function to remove leading and trailing spaces from a string
void removeLeadingTrailingSpaces(char * str) {
  int i = 0, j = strlen(str) - 1;

  // Remove leading spaces
  while (isspace(str[i])) {
    i++;
  }

  // Remove trailing spaces
  while (j >= 0 && isspace(str[j])) {
    j--;
  }

  // Shift the non-space characters to the beginning of the string
  memmove(str, str + i, j - i + 1);

  // Null-terminate the string
  str[j - i + 1] = '\0';
}
// Function to extract root operation from command
char * get_root_operation(char * command) {
  char * str;

  // Find the first space character in the command
  str = strtok(command, " ");
  if (str != NULL) {
    removeLeadingTrailingSpaces(str);
    return str;
  }

  return NULL;
}
// Function to find files within a size range in a directory
int find_files_of_sizes(const char * dir_path, int size1, int size2, int * num_of_files) {
  DIR * direc;
  struct dirent * direc_entry;
  char buffer[BUFFER_SIZE];
  FILE * fp;

  // In case there is an issue while opening the directory.
  if ((direc = opendir(dir_path)) == NULL) {
    printf("Error while opening the directory.\n");
    return 1;
  }

  // Opening "TEMP_DUMP_TXT" in the append mode.
  fp = fopen(TEMP_DUMP_TXT, "a");
  if (fp == NULL) {
    fprintf(stderr, "Error during the opening of file.\n");
    return 1;
  }

  while ((direc_entry = readdir(direc)) != NULL) {
    // Omiting the Directories with single dot(.) and two consecutive dots(..)
    if (strcmp(direc_entry -> d_name, ".") == 0 || strcmp(direc_entry -> d_name, "..") == 0) {
      continue;
    }

    snprintf(buffer, BUFFER_SIZE, "%s/%s", dir_path, direc_entry -> d_name);

    // Fetching the stats of File and Directories.
    struct stat file_state;
    if (stat(buffer, & file_state) == -1) {
      continue;
    }

    // In case it is directory then perform search operation recursively.
    if (S_ISDIR(file_state.st_mode)) {
      find_files_of_sizes(buffer, size1, size2, num_of_files);
    }
    // In case it is a file then checking its size if its in within the specified limit.
    else if (S_ISREG(file_state.st_mode) && file_state.st_size >= size1 && file_state.st_size <= size2 && size1 <= size2) {
      fprintf(fp, "%s\n", buffer);
      ( * num_of_files) ++;
    }
  }

  closedir(direc);
  fclose(fp);
  return 0;
}
// Function to extract sizes from command
void get_sizes(int * size1, int * size2, char * command_sz) {
  // Extract the first token
  char * ptr = strtok(command_sz, " ");
  if (ptr != NULL) {
    // Extract the second token
    ptr = strtok(NULL, " ");
    if (ptr != NULL) {
      * size1 = atoi(ptr); // Convert the token to an integer
      // Extract the third token
      ptr = strtok(NULL, " ");
      if (ptr != NULL) {
        * size2 = atoi(ptr); // Convert the token to an integer
      } else {
        * size2 = 0; // If no third token, set size2 to 0
      }
    } else {
      * size1 = 0; // If no second token, set size1 to 0
    }
  }
}
// Function to compress files into a tarball
int compress_files(char file_paths[DIR_THRESHOLD_MAX][LENGTH_MAX], int num_of_files) {
  char tar_command[LENGTH_MAX * DIR_THRESHOLD_MAX + 100]; // Allocate enough space for the command
  const char * directory = "w24project";
  if (access(directory, F_OK) != 0) {
    if (mkdir(directory, 0777) != 0) {
      fprintf(stderr, "Error: Failed to create w24project directory\n");
      return 1;
    }
  }
  // Construct the tar command to compress the files into a tarball
  snprintf(tar_command, sizeof(tar_command), "tar -czf %s/temp.tar.gz", directory);

  // Add each file path to the tar command
  for (int i = 0; i < num_of_files; i++) {
    strcat(tar_command, " ");
    strcat(tar_command, file_paths[i]);
  }
  sleep(1); // Delay for 1 second
  // Execute the tar command using system call
  int status = system(tar_command);

  if (status == 0) {
    printf("Files compressed successfully\n");
    return 0;
  } else {
    fprintf(stderr, "Error compressing files\n");
    return -1;
  }
}
// Function to find files created before or on a specified date
int find_files_before_date(const char * dir_path,
  const char * date, char file_paths[DIR_THRESHOLD_MAX][LENGTH_MAX], int * num_of_files) {
  struct tm tm_date;
  memset( & tm_date, 0, sizeof(struct tm)); // Initialize tm_date to avoid potential issues
  strptime(date, "%Y-%m-%d", & tm_date); // Include <time.h> to recognize strptime
  time_t target_time = mktime( & tm_date);

  DIR * directory = opendir(dir_path);
  if (directory == NULL) {
    perror("Error opening directory");
    return 1;
  }

  struct dirent * entry;

  while ((entry = readdir(directory)) != NULL && * num_of_files < DIR_THRESHOLD_MAX) {
    if (strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0) {
      continue;
    }

    char file_path[LENGTH_MAX + 1]; // Increase buffer size by 1 for null terminator
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry -> d_name); // Use sizeof to ensure buffer safety

    struct stat file_stat;
    if (stat(file_path, & file_stat) == -1) {
      perror("Error getting file status");
      continue;
    }

    if (difftime(file_stat.st_ctime, target_time) <= 0) {
      // File created before or on the specified date
      strncpy(file_paths[ * num_of_files], file_path, LENGTH_MAX);
      ( * num_of_files) ++;
    }
  }

  closedir(directory);
  return 0;
}

int find_files_after_date(const char * dir_path,
  const char * date, char file_paths[DIR_THRESHOLD_MAX][LENGTH_MAX], int * num_of_files) {
  struct tm tm_date;
  memset( & tm_date, 0, sizeof(struct tm)); // Initialize tm_date to avoid potential issues
  if (strptime(date, "%Y-%m-%d", & tm_date) == NULL) {
    perror("Error parsing date");
    return 1;
  }
  time_t target_time = mktime( & tm_date);

  time_t current_time = time(NULL);
  time_t two_minutes_before = current_time - 30; // 2 minutes before the current time

  DIR * directory = opendir(dir_path);
  if (directory == NULL) {
    perror("Error opening directory");
    return 1;
  }

  struct dirent * entry;

  while ((entry = readdir(directory)) != NULL && * num_of_files < DIR_THRESHOLD_MAX) {
    if (entry -> d_name[0] == '.') {
      continue; // Skip dot files or hidden files
    }

    char file_path[LENGTH_MAX + 1]; // Increase buffer size by 1 for null terminator
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry -> d_name); // Use sizeof to ensure buffer safety

    struct stat file_stat;
    if (stat(file_path, & file_stat) == -1) {
      perror("Error getting file status");
      continue;
    }

    if (difftime(file_stat.st_ctime, target_time) >= 0 && difftime(file_stat.st_ctime, two_minutes_before) <= 0) {
      // File created from the given date till 2 minutes before the execution of the code
      strncpy(file_paths[ * num_of_files], file_path, LENGTH_MAX);
      ( * num_of_files) ++;
    }
  }

  closedir(directory);
  return 0;
}

// Function to parse date command and call appropriate function
void handle_date_command(const char * command, int client_socket) {
  char date[INPUT_THREASHOLD];
  sscanf(command, "%*s %s", date); // Extract the date from the command

  // Determine the operation based on the command
  if (strncmp(command, "w24fdb", 6) == 0) {
    int num_of_files = 0;
    char file_paths[DIR_THRESHOLD_MAX][LENGTH_MAX];
    find_files_before_date(getenv("HOME"), date, file_paths, & num_of_files);
    // Compress and send the files
    if (compress_files(file_paths, num_of_files) == 0) {
      send_response(ZIP_FILE_NAME, RESPONSE_ZIP, client_socket);
    } else {
      send_response("Error compressing files", RESPONSE_TEXT, client_socket);
    }
  } else if (strncmp(command, "w24fda", 6) == 0) {
    int num_of_files = 0;
    char file_paths[DIR_THRESHOLD_MAX][LENGTH_MAX];
    find_files_after_date(getenv("HOME"), date, file_paths, & num_of_files);
    // Compress and send the files
    if (compress_files(file_paths, num_of_files) == 0) {
      send_response(ZIP_FILE_NAME, RESPONSE_ZIP, client_socket);
    } else {
      send_response("Error compressing files", RESPONSE_TEXT, client_socket);
    }
  } else {
    send_response("Invalid command", RESPONSE_TEXT, client_socket);
  }
}
// Function to handle client requests
void crequest(int client_socket) {
  char command[INPUT_THREASHOLD];
  int n;
  bool is_ndir = true;
  while (1) {
    // Receive command from client
    memset(command, 0, sizeof(command));
    if ((n = recv(client_socket, command, sizeof(command), 0)) <= 0) {
      if (n == 0) {
        printf("Client disconnected\n");
      } else {
        perror("Receive failed");
      }
      close(client_socket);
      return; // Terminate the function after closing the socket
    }

    // Parse command and perform action
    if (strncmp(command, "quitc", 5) == 0) {
      // Client requested to quit
      printf("Client requested to quit\n");
      // Optionally, send a response to the client indicating it can close the connection
      char * quit_msg = "Server closing connection.";
      send_response(quit_msg, RESPONSE_TEXT, client_socket);
      close(client_socket);
      return; // Terminate the function after closing the socket
    } else if (strcmp(command, "dirlist -a\n") == 0) {
      // Execute command and send output to client
      FILE * fp;
      char path[1035];
      int bytes_read;
      is_ndir = false;
      /* Open the command for reading. */
      fp = popen("ls -l ~/ | grep '^d' | awk '{print $NF}' | sort", "r");
      if (fp == NULL) {
        printf("Command failure\n");
        exit(1);
      }

      /* Read the output until the end of the stream. */
      while ((bytes_read = read(fileno(fp), path, sizeof(path) - 1)) != 0) {
        if (bytes_read == -1) {
          perror("read");
          exit(1);
        }
        path[bytes_read] = '\0'; // Null-terminate the string
        //send(client_socket, path, strlen(path), 0);
        send_response(path, RESPONSE_TEXT, client_socket);
      }

      /* Close the pipe */
      pclose(fp);
    } else if (strcmp(command, "dirlist -t\n") == 0) {
      //find ~/ -type d -printf "%T@ %p\n" | sort -n | cut -d' ' -f2-
      is_ndir = false;
      FILE * fp;
      char path[1035];
      int bytes_read;

      /* Open the command for reading. */
      fp = popen("ls -lt $HOME | grep '^d' | awk '{print $NF}'", "r");
      if (fp == NULL) {
        printf("Command failure\n");
        exit(1);
      }

      /* Read the output until the end of the stream. */
      while ((bytes_read = read(fileno(fp), path, sizeof(path) - 1)) != 0) {
        if (bytes_read == -1) {
          perror("read");
          exit(1);
        }
        path[bytes_read] = '\0'; // Null-terminate the string
        send_response(path, RESPONSE_TEXT, client_socket);
      }

      /* Close the pipe */
      pclose(fp);
    } else { //starts

      char command_copy[INPUT_THREASHOLD];
      strncpy(command_copy, command, INPUT_THREASHOLD);
      command_copy[INPUT_THREASHOLD - 1] = '\0'; // Ensure null-termination

      char command_copy1[INPUT_THREASHOLD];
      strncpy(command_copy1, command, INPUT_THREASHOLD);
      command_copy1[INPUT_THREASHOLD - 1] = '\0';

      char * operation = get_root_operation(command_copy);
      printf("%s operation is going to execute...\n", operation);

      if (strcmp(operation, "w24fn") == 0) {
        char * token = strtok(command_copy1, " ");
        char file_name[LENGTH_MAX]; // Use a statically allocated array
        printf("token1: %s\n", token);
        if (token != NULL) {
          token = strtok(NULL, " ");
          printf("token2: %s\n", token);
          if (token != NULL) { // Ensure token is not NULL before copying
            strncpy(file_name, token, LENGTH_MAX);
            file_name[LENGTH_MAX - 1] = '\0'; // Ensure null-termination
          } else {
            file_name[0] = '\0'; // If no token, set file_name to empty string
          }
        }

        removeLeadingTrailingSpaces(file_name);
        printf("The entered filename is %s\n", file_name);

        char output[BUFFER_SIZE];
        FILE * fp;

        // Construct the command to find the file information
        snprintf(output, sizeof(output), "find ~/ -type f -name \"%s\" -exec ls -l --time-style=full-iso {} + | head -n 1 | awk '{print $NF, $5, $6, $1}'", file_name);

        // Open a pipe to the command for reading its output
        fp = popen(output, "r");
        if (fp == NULL) {
          perror("popen");
          exit(EXIT_FAILURE);
        }
        char contents[BUFFER_SIZE];
        ssize_t bytes_read;
        //memset(contents, 0, sizeof(contents));
        bytes_read = read(fileno(fp), contents, sizeof(contents));
        contents[bytes_read] = '\0';
        if (bytes_read == 0) {
          printf("File not found\n");
        } else {
          //send(client_socket, contents, strlen(contents), 0);
          send_response(contents, RESPONSE_TEXT, client_socket);
        }

        // Close the pipe
        pclose(fp);
      } else if (strcmp(operation, "w24fz") == 0) {

        int size1 = 0;
        int size2 = 0;

        get_sizes( & size1, & size2, command_copy1);

        if (access("w24project", F_OK) != 0) {
          if (mkdir("w24project", 0777) != 0) {
            fprintf(stderr, "Error: Failed to create w24project directory\n");
            return 1;
          }
        }

        char buffer[BUFFER_SIZE];
        char * tar_command = "tar -czf %s -T %s"; // Tar Command format.
        int status, num_of_files = 0;

        // Fetching the files.
        find_files_of_sizes(getenv("HOME"), size1, size2, & num_of_files);
        sprintf(buffer, tar_command, ZIP_FILE_NAME, TEMP_DUMP_TXT);

        if (num_of_files > 0) {
          int status = system(buffer);
          if (status == 0) {
            // Sending the file(compressed) from Server -> Client.
            int tr_status = send_response(ZIP_FILE_NAME, RESPONSE_ZIP, client_socket);
            if (tr_status != 0) {
              printf("Error occured in transferring the file(s).\n");
            } else {
              printf("File transfer successfully completed.\n");
            }
          } else {
            printf("Error in executing the command.\n");
          }
        } else {
          // Sending a response in the text form in case no file is located/matched.
          printf("No files found.\n");
          const char * directory_name = "w24project";

          // Attempt to remove the directory
          if (rmdir(directory_name) == 0) {
            printf("Directory '%s' removed successfully.\n", directory_name);
          }
          send_response("No files found.", RESPONSE_TEXT, client_socket);
        }

        // Now, deleting the file named - "temporary_file_list.txt"
        status = remove(TEMP_DUMP_TXT);
        if (status != 0) {
          printf("Error while deleting the text file\n");
        }
      } else if (strncmp(operation, "w24ft", 5) == 0) {
        char * extensions[3] = {
          NULL,
          NULL,
          NULL
        };
        int ext_count = 0;
        char * token = strtok(NULL, " "); // Skip the command itself

        // Collect up to 3 extensions
        while (token != NULL && ext_count < 3) {
          extensions[ext_count++] = token;
          token = strtok(NULL, " ");
        }

        // Check if at least one extension is provided
        if (ext_count == 0) {
          send_response("No file extensions provided", RESPONSE_TEXT, client_socket);
          return;
        }
        if (access("w24project", F_OK) != 0) {
          if (mkdir("w24project", 0777) != 0) {
            fprintf(stderr, "Error: Failed to create w24project directory\n");
            return 1;
          }
        }

        // Construct the find command with the specified extensions
        char find_command[BUFFER_SIZE] = "find ~/ -type f \\( ";
        for (int i = 0; i < ext_count; i++) {
          if (i > 0) {
            strcat(find_command, "-o ");
          }
          strcat(find_command, "-name '*.");
          strcat(find_command, extensions[i]);
          strcat(find_command, "' ");
        }
        strcat(find_command, "\\) > ");
        strcat(find_command, TEMP_DUMP_TXT);
        printf("\ncc: %s\n", find_command);

        // Execute the find command
        int find_status = system(find_command);
        if (find_status != 0) {
          printf("Error executing find command\n");
          send_response("Error executing find command", RESPONSE_TEXT, client_socket);
          return;
        }

        // Check if TEMP_DUMP_TXT is empty
        struct stat st;
        stat(TEMP_DUMP_TXT, & st);
        if (st.st_size == 0) {
          const char * directory_name = "w24project";

          // Attempt to remove the directory
          if (rmdir(directory_name) == 0) {
            printf("Directory '%s' removed successfully.\n", directory_name);
          }
          // No files found, send message to client
          send_response("No file found.", RESPONSE_TEXT, client_socket);
        } else {
          // Files found, create tarball and send
          char tar_command[BUFFER_SIZE];
          snprintf(tar_command, sizeof(tar_command), "tar -czf %s -T %s", ZIP_FILE_NAME, TEMP_DUMP_TXT);
          int tar_status = system(tar_command);
          if (tar_status == 0) {
            // Tarball created successfully, send it to the client
            send_response(ZIP_FILE_NAME, RESPONSE_ZIP, client_socket);
          } else {
            printf("Error creating tar\n");
            send_response("Error creating tar", RESPONSE_TEXT, client_socket);
          }
        }

        // Clean up
        remove(TEMP_DUMP_TXT);
      } else if (strncmp(command, "w24fdb", 6) == 0 || strncmp(command, "w24fda", 6) == 0) {
        handle_date_command(command, client_socket);
      } else {
        send_response("Invalid command", RESPONSE_TEXT, client_socket);
      }
    }
  }
}

int main() {
  int server_socket, client_socket;
  struct sockaddr_in address;
  struct sockaddr_in dest;
  int addrlen = sizeof(address);
  int child_status;
  // Create socket
  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }
  memset( & address, 0, sizeof(address));
  memset( & dest, 0, sizeof(dest));
  // Bind socket to port
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  int opt = 1;
  // Set SO_REUSEADDR on a socket to true (1):
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, & opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  if (bind(server_socket, (struct sockaddr * ) & address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_socket, 3) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }
  printf("Mirror 1 is ready, waiting for the client to connect\n");
  // Accept client connections
  while (1) {
    connection_counter++;
    client_socket = accept(server_socket, (struct sockaddr * ) NULL, NULL);
    printf("Client %d is connected successfully.\n", connection_counter);

    // Now, Forking of a child process for handling requests from client.
    if (!fork()) {
      crequest(client_socket);
      close(client_socket);
      exit(0);
    }
    waitpid(0, & child_status, WNOHANG);
  }

  return 0;
}