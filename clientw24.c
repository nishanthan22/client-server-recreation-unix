/*Required headers*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
//Global variables required for the program
#define THRESHOLD_LOW 0 // Minimum size in bytes (1 KB)
#define THRESHOLD_HIGH INT_MAX // Maximum size using the maximum value for int
#define PORT 4222
#define INPUT_THREASHOLD 100
#define BUFFER_SIZE 1024
#define ARGUMENT_THRESHOLD 5
#define RESPONSE_TEXT 1
#define RESPONSE_ZIP 2

#define HOSTNAME_MIRROR "192.168.6.129"
#define MIRROR_1_PORT 54321
#define MIRROR_2_PORT 54325
//Function to send the message to server
void send_command_to_server(int client_socket, char * command) {
  // Send command to server
  if (send(client_socket, command, strlen(command), 0) < 0) {
    perror("Send failed");
    close(client_socket);
    exit(EXIT_FAILURE);
  } else {
    printf("Command successfully sent to server\n");
  }
}
//Function to make the connection to the server
int establish_connection_to_mirror(int port_number, int mirror) {
  int client_socket;
  struct sockaddr_in server_address;
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Reading IP address and port number
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_number);
  if (inet_pton(AF_INET, HOSTNAME_MIRROR, &server_address.sin_addr) <= 0) {
    perror("Invalid address/ Address not supported");
    exit(EXIT_FAILURE);
  }

  // Connecting to the Mirror Server
  if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    perror("Connection to mirror server failed");
    exit(EXIT_FAILURE);
  }
  printf("Connection established to the mirror %d server successfully.\n", mirror);
  return client_socket; // Return the new socket descriptor
}
// Function to receive response from the server
int receive_response_from_server(int client_socket) {
  int type;
  ssize_t bytes_received;
  char buffer[BUFFER_SIZE];
  struct timeval tv;
  fd_set readfds;

  // Set timeout for receiving data (e.g., 5 seconds)
  tv.tv_sec = 5;
  tv.tv_usec = 0;

  FD_ZERO(&readfds);
  FD_SET(client_socket, &readfds);

  // Wait for the socket to be ready for reading
  int select_result = select(client_socket + 1, &readfds, NULL, NULL, &tv);
   if (select_result == -1) {
    perror("select error");
    return client_socket;
  } else if (select_result == 0) {
    printf("Timeout occurred! No data received.\n");
    return client_socket;
  }
    if (FD_ISSET(client_socket, &readfds)) {
  // Receive response type
  bytes_received = recv(client_socket, & type, sizeof(type), 0);
  if (bytes_received <= 0) {
    perror("Failed to receive the type of response");
    return client_socket; // Return the original client socket
  }

  if (type == RESPONSE_TEXT) {
    // Receive text response
    memset(buffer, 0, BUFFER_SIZE); // Clear buffer
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
      perror("Failed to receive text response");
      return client_socket; // Return the original client socket
    }
    buffer[bytes_received] = '\0'; // Null-terminate the string
    printf("%s\n", buffer);
    
    if (strcmp(buffer, "Server closing connection.") == 0) {
      close(client_socket);
      printf("Connection termination success\n");
      exit(EXIT_SUCCESS);
    }
    if (strcmp(buffer, "M1") == 0) {
      close(client_socket);
      establish_connection_to_mirror(MIRROR_1_PORT, client_socket);
    }
    if (strcmp(buffer, "M2") == 0) {
      close(client_socket);
      establish_connection_to_mirror(MIRROR_2_PORT, client_socket);
    }
  } else if (type == RESPONSE_ZIP) {
    // Receive file contents
    FILE * fp = fopen("received_file.tar.gz", "wb");
    if (fp == NULL) {
      perror("Failed to open file for writing");
      return client_socket; // Return the original client socket
    }

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
      fwrite(buffer, 1, bytes_received, fp);
    }

    if (bytes_received < 0) {
      perror("Failed to receive file contents");
    } else {
      printf("File received successfully.\n");
    }

    fclose(fp);
  } }

  // Return the original client socket if no redirection occurred
  return client_socket;
}
// Function to validate size argument
bool validate_size_arg(int size, char * size_str) {
  if (size < THRESHOLD_LOW || size > THRESHOLD_HIGH) {
    printf("Invalid size. Size must be within the range [%d, %d]\n", THRESHOLD_LOW, THRESHOLD_HIGH);
    return false;
  }
  for (int i = 0; size_str[i] != '\0'; i++) {
    if (!isdigit(size_str[i])) {
      printf("size: %s is not in digits\n", size_str);
      return false; // Not all characters are digits
    }
  }
  return true; // All characters are digits
}
// Function to parse and audit date
bool parse_and_audit_date(char date[]) {
  int YYYY = 0, MM = 0, DD = 0;
  int yM = 1000, mM = 10;
  int i = 0;

  // Parse the year
  for (i = 0; i < 4; i++) {
    if (date[i] < '0' || date[i] > '9')
      return false;
    YYYY += (date[i] - '0') * yM;
    yM /= 10;
  }

  // Validate the hyphen separator
  if (date[i] != '-') {
    printf("Invalid delimeter in given date\n");
    return false;
  }
  i++;

  // Parse the month
  for (; i < 7; i++) {
    if (date[i] < '0' || date[i] > '9')
      return false;
    MM += (date[i] - '0') * mM;
    mM /= 10;
  }

  // Validate the hyphen separator
  if (date[i] != '-') {
    printf("Invalid delimeter in given date\n");
    return false;
  }
  i++;

  // Parse the day
  for (; i < 10; i++) {
    if (date[i] < '0' || date[i] > '9')
      return false;
    DD = DD * 10 + (date[i] - '0');
  }

  // Check if the year is within valid range (between 1000 and 2024)
  if (YYYY < 1000 || YYYY > 2024) {
    return false;
  }

  // Check if the month is within valid range (between 1 and 12)
  if (MM < 1 || MM > 12) {
    return false;
  }

  // Check if the day is within valid range (between 1 and 31)
  if (DD < 1 || DD > 31) {
    return false;
  }

  // Check for months with 30 days
  if ((MM == 4 || MM == 6 || MM == 9 || MM == 11) && DD > 30) {
    return false;
  }

  // Check for February
  if (MM == 2) {
    // Leap year
    if ((YYYY % 4 == 0 && YYYY % 100 != 0) || YYYY % 400 == 0) {
      if (DD > 29) {
        return false;
      }
    }
    // Non-leap year
    else {
      if (DD > 28) {
        return false;
      }
    }
  }

  // If all checks pass, the date is valid
  return true;
}
// Function to audit command
bool audit_command(char * command) {
  // Trim the newline character from the command
  size_t length = strlen(command);
  if (length > 0 && command[length - 1] == '\n') {
    command[length - 1] = '\0';
  }

  char * tokens[ARGUMENT_THRESHOLD];
  int numTokens = 0;

  // Use strtok to split input by space delimiter
  char * token = strtok(command, " ");

  while (token != NULL) {
    // Save each token 
    tokens[numTokens++] = token;
    // Get next token
    token = strtok(NULL, " ");
  }

  // Add null at end
  tokens[numTokens] = NULL;

  // Check if command entered is valid
  if (strcmp(tokens[0], "dirlist") != 0 && //check command in client
    strcmp(tokens[0], "w24fn") != 0 &&
    strcmp(tokens[0], "w24fz") != 0 &&
    strcmp(tokens[0], "w24ft") != 0 &&
    strcmp(tokens[0], "w24fdb") != 0 &&
    strcmp(tokens[0], "w24fda") != 0 &&
    strcmp(tokens[0], "quitc") != 0) {

    printf("Warning  -----> The command entered is not valid. Please try again with a valid command.\n");
    return false;
  }
  // Checking the provided command as a arguments
  if (strcmp(tokens[0], "quitc") != 0 && (numTokens < 2 || numTokens > ARGUMENT_THRESHOLD)) {
    printf("Please provide the required arguments (only) to process.\n");
    return false;
  }
  if (strcmp(tokens[0], "dirlist") == 0) {
    if (numTokens > 2) {
      printf("The dirlist command accepts only one option\n");
      return false;
    } else if (strcmp(tokens[1], "-a") != 0 && strcmp(tokens[1], "-t") != 0) {
      printf("You have entered an invalid option for dirlist command\n");
      return false;
    }
  }
  if (strcmp(tokens[0], "w24fn") == 0 && numTokens > 2) {
    printf("The w24fn command accepts only one option: filename\n");
    return false;
  }
  // Checking the command arguments as per requirements and display the appropriate messages.
  if (strcmp(tokens[0], "w24fz") == 0) {

    if (numTokens != 3) {
      printf("The w24fz command needs 3 arguments to process.\n");
      return false;
    }
    int size1 = atoi(tokens[1]);
    int size2 = atoi(tokens[2]);
    bool is_size1_valid = validate_size_arg(size1, tokens[1]);
    bool is_size2_valid = validate_size_arg(size2, tokens[2]);

    if (!is_size1_valid || !is_size2_valid)
      return false;

    if (size1 > size2) {
      printf("Invalid sizes. size2 must be greater than or equal to size1\n");
      return false;
    }

  }
  //verify the command arguments in case of 'fgets' command
  if (strcmp(tokens[0], "w24ft") == 0 && (numTokens < 2 || numTokens > 4)) {
    printf("w24ft command allows minimum 1 file names and maximum 3 extensions.\n");
    return false;
  }
  //verify the command arguments in case of 'fgets' command
  if (strcmp(tokens[0], "w24fdb") == 0) {
    if (numTokens > 2) {
      printf("The w24fdb command accepts only one date\n");
      return false;
    }
    // Validateing date formats
    if (!parse_and_audit_date(tokens[1])) {
      printf("Invalid date arguments. Accepted date format is YYYY-MM-DD\n");
      return false;
    }
  }
  if (strcmp(tokens[0], "w24fda") == 0) {
    if (numTokens > 2) {
      printf("The w24fda command accepts only one date\n");
      return false;
    }
    // Validateing date formats
    if (!parse_and_audit_date(tokens[1])) {
      printf("Invalid date arguments. Accepted date format is YYYY-MM-DD\n");
      return false;
    }
  }
  if (strcmp(tokens[0], "quitc") == 0 && numTokens > 1) {
    printf("quitc does not take any arguments.\n");
    return false;
  }

  return true;
}

int main(int argc, char * argv[]) {
  int client_socket;
  struct sockaddr_in server_address;
  struct hostent * he;

  if (argc != 2) {
    fprintf(stderr, "Please input the client name and hostname\n");
    exit(EXIT_FAILURE);
  }

  if ((he = gethostbyname(argv[1])) == NULL) {
    fprintf(stderr, "Cannot get host name\n");
    exit(EXIT_FAILURE);
  }

  // Create socket
  if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }
  memset( & server_address, 0, sizeof(server_address));
  // Specify server details
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr = * ((struct in_addr * ) he -> h_addr);

  // Connect to server
  if (connect(client_socket, (struct sockaddr * ) & server_address, sizeof(server_address)) < 0) {
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Get user input
    char command[INPUT_THREASHOLD];
    printf("clientw24$: ");
    fgets(command, sizeof(command), stdin);

    // Create a copy of the command for auditing
    char command_copy[INPUT_THREASHOLD];
    strncpy(command_copy, command, INPUT_THREASHOLD);
    command_copy[INPUT_THREASHOLD - 1] = '\0'; // Ensure null-termination

    bool is_valid_command = audit_command(command_copy);
    // Send command to server
    if (is_valid_command)
      send_command_to_server(client_socket, command); // Use original command here
    else {
      printf("Invalid command. Please try again.\n");
      continue; // Prompt for a new command
    }

    // Receive response from server
    client_socket = receive_response_from_server(client_socket);
  }

  return 0;
}