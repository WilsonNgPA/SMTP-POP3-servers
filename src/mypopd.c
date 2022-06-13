#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);
static int parse_int(const char * str);

int main(int argc, char *argv[]) {
  
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
    return 1;
  }
  
  run_server(argv[1], handle_client);
  
  return 0;
}

void handle_client(int fd) {
  
  char recvbuf[MAX_LINE_LENGTH + 1];
  net_buffer_t nb = nb_create(fd, MAX_LINE_LENGTH);
  
  /* TO BE COMPLETED BY THE STUDENT */
  send_formatted(fd, "+OK POP3 server ready\r\n");

  char * command, * command_saveptr;
  int authorization_state = 1;
  int quit_received = 0;
  int user_received = 0;

  char user[MAX_LINE_LENGTH];
  while (authorization_state == 1) {
    nb_read_line(nb, recvbuf);
    if (strchr(recvbuf, ' ')) {
      command = strtok_r(recvbuf, " ", &command_saveptr);
    }
    else {
      command = strtok_r(recvbuf, "\r\n", &command_saveptr);
    }
    if (strcasecmp(command, "USER") == 0) {
      char * username = strtok_r(NULL, "\r\n", &command_saveptr);
      if (username == NULL || username[0] == '\0') {
        send_formatted(fd, "-ERR no user entered\r\n");
        user_received = 0;
      }
      else {
        if (is_valid_user(username, NULL) != 0) {
          strcpy(user, username);
          send_formatted(fd, "+OK %s is a valid mailbox\r\n", username);
          user_received = 1;
        }
        else {
          send_formatted(fd, "-ERR %s is not recognized\r\n", username);
          user_received = 0;
        }
      }
    }
    else if (strcasecmp(command, "PASS") == 0) {
      if (user_received == 1) {
        char * pass = strtok_r(NULL, "\r\n", &command_saveptr);
        if (pass == NULL || pass[0] == '\0') {
          send_formatted(fd, "-ERR no pass entered\r\n");
        }
        else {
          if (is_valid_user(user, pass) != 0) {
            authorization_state = 0;
          }
          else {
            send_formatted(fd, "-ERR invalid user and password combination\r\n");
          }
        }
      }
      else {
        send_formatted(fd, "-ERR PASS entered before USER\r\n");
      }
    }
    else if (strcasecmp(command, "QUIT") == 0) {
      quit_received = 1;
      authorization_state = 0;
    }
    else {
      send_formatted(fd, "-ERR invalid command\r\n");
    }
  }

  if (quit_received == 1) {
    nb_destroy(nb);
    return;
  }

  mail_list_t user_mail = load_user_mail(user);
  unsigned int initial_count = get_mail_count(user_mail);
  send_formatted(fd, "+OK %s's maildrop has %i messages (%li octets)\r\n", user, initial_count, get_mail_list_size(user_mail));
  
  int transaction_state = 1;
  while (transaction_state == 1) {
    nb_read_line(nb, recvbuf);
    if (strchr(recvbuf, ' ')) {
      command = strtok_r(recvbuf, " ", &command_saveptr);
    }
    else {
      command = strtok_r(recvbuf, "\r\n", &command_saveptr);
    }
    if (strcasecmp(command, "STAT") == 0) {
      send_formatted(fd, "+OK %i %li\r\n", get_mail_count(user_mail), get_mail_list_size(user_mail));
    }
    else if (strcasecmp(command, "LIST") == 0) {
      char * message = strtok_r(NULL, "\r\n", &command_saveptr);
      if (message == NULL || message[0] == '\0') {
        send_formatted(fd, "+OK %i messages (%li octets)\r\n", get_mail_count(user_mail), get_mail_list_size(user_mail));
        for (unsigned int i = 0; i < initial_count; i++) {
          if (get_mail_item(user_mail, i) != NULL) {
            send_formatted(fd, "%i %li\r\n", i + 1, get_mail_item_size(get_mail_item(user_mail, i)));
          }
        }
        send_formatted(fd, ".\r\n"); 
      }
      else {
        int message_num = parse_int(message);
        if (message_num < 0) {
          send_formatted(fd, "-ERR invalid message number entered\r\n");
        }
        else {
          if (get_mail_item(user_mail, message_num) != NULL) {
            send_formatted(fd, "+OK %i %li\r\n", message_num, get_mail_item_size(get_mail_item(user_mail, message_num)));
          }
          else {
            send_formatted(fd, "-ERR no such message\r\n");
          }
        }
      } 
    }
    else if (strcasecmp(command, "RETR") == 0) {
      char * message = strtok_r(NULL, "\r\n", &command_saveptr);
      if (message == NULL || message[0] == '\0') {
        send_formatted(fd, "-ERR message number not entered\r\n");
      }
      else {
        int message_num = parse_int(message);
        if (message_num < 0) {
          send_formatted(fd, "-ERR invalid message number entered\r\n");
        }
        else {
          if (get_mail_item(user_mail, message_num) != NULL) {
            send_formatted(fd, "+OK %i %li octets\r\n", message_num, get_mail_item_size(get_mail_item(user_mail, message_num)));
            char * buffer = 0;
            long length;
            FILE * message_file = get_mail_item_contents(get_mail_item(user_mail, message_num));
            if (message_file) {
              fseek(message_file, 0, SEEK_END);
              length = ftell(message_file);
              fseek(message_file, 0, SEEK_SET);
              buffer = malloc(length);
              if (buffer) {
                fread(buffer, 1, length, message_file);
              }
              fclose(message_file);
            }
            send_formatted(fd, "%s", buffer);
            free(buffer);
            send_formatted(fd, ".\r\n");
          }
          else {
            send_formatted(fd, "-ERR no such message\r\n");
          }
        }
      }
    }
    else if (strcasecmp(command, "DELE") == 0) {
      char * message = strtok_r(NULL, "\r\n", &command_saveptr);
      if (message == NULL || message[0] == '\0') {
        send_formatted(fd, "-ERR message number not entered\r\n");
      }
      else {
        int message_num = parse_int(message);
        if (message_num < 0) {
          send_formatted(fd, "-ERR invalid message number entered\r\n");
        }
        else {
          if (get_mail_item(user_mail, message_num - 1) != NULL) {
            mark_mail_item_deleted(get_mail_item(user_mail, message_num - 1));
            send_formatted(fd, "+OK message %i marked as deleted\r\n", message_num);
          }
          else {
            send_formatted(fd, "-ERR message %i does not exist\r\n", message_num);
          }
        }
      }
    }
    else if (strcasecmp(command, "NOOP") == 0) {
      send_formatted(fd, "+OK\r\n");
    }
    else if (strcasecmp(command, "RSET") == 0) {
      reset_mail_list_deleted_flag(user_mail);
      send_formatted(fd, "+OK maildrop has %i messages (%li octets)\r\n", get_mail_count(user_mail), get_mail_list_size(user_mail));
    }
    else if (strcasecmp(command, "QUIT") == 0) {
      transaction_state = 0;
      destroy_mail_list(user_mail);
    }
    else {
      send_formatted(fd, "-ERR invalid command\r\n");
    }
  }
  send_formatted(fd, "+OK %s POP3 server signing off\r\n", user);
  nb_destroy(nb);
  return;
}

int parse_int(const char * str) {
  char * endptr;
  errno = 0;
  long message_num = strtol(str, &endptr, 10);
  if (errno == ERANGE || *endptr != '\0' || str == endptr) {
    return -1;
  }
  return (int) message_num;
}
