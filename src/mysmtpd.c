#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);

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

  struct utsname my_uname;
  uname(&my_uname);

  
  /* TO BE COMPLETED BY THE STUDENT */
  send_formatted(fd, "220 %s Server ready\r\n", my_uname.__domainname);

  char * command, * command_saveptr;
  int received_greeting = 0;
  int quit_received = 0;

  while (quit_received == 0) {
    nb_read_line(nb, recvbuf);
    if (strchr(recvbuf, ' ')) {
      command = strtok_r(recvbuf, " ", &command_saveptr);
    }
    else {
      command = strtok_r(recvbuf, "\r\n", &command_saveptr);
    }
    if (strcasecmp(command, "EHLO") == 0 || strcasecmp(command, "HELO") == 0) {
      char * name = strtok_r(NULL, "\r\n", &command_saveptr);
      if (name == NULL || name[0] == '\0') {
        send_formatted(fd, "501 Syntax error in parameters or arguments (EHLO/HELO no paramaters or arguments)\r\n");
      }
      else { 
        send_formatted(fd, "250-%s greets %s\r\n", my_uname.__domainname, name);
        received_greeting = 1;
      }
    }
    else if (strcasecmp(command, "RSET") == 0) {
      send_formatted(fd, "250 OK\r\n");
    } 
    else if (strcasecmp(command, "NOOP") == 0) {
      send_formatted(fd, "250 OK\r\n");
    }
    else if (strcasecmp(command, "VRFY") == 0) {
      char * user = strtok_r(NULL, "\r\n", &command_saveptr);
      if (is_valid_user(user, NULL) != 0) {
        send_formatted(fd, "250 %s\r\n", user);
      } 
      else {
        send_formatted(fd, "550 No such user here\r\n");
      }
    } 
    else if (strcasecmp(command, "QUIT") == 0) {
      quit_received = 1;
    } 
    else {
      if (received_greeting == 0) {
        send_formatted(fd, "503 Bad sequence of commands\r\n");
      } 
      else {
        if (strcasecmp(command, "MAIL") == 0) {
          char * syntax_check_mail_saveptr;
          char * syntax_check_mail = strtok_r(NULL, "\r\n", &command_saveptr);
          if (strchr(syntax_check_mail, '<') && strchr(syntax_check_mail, '>')) {
            if (strcasecmp(strtok_r(syntax_check_mail, "<", &syntax_check_mail_saveptr), "FROM:") == 0) {
              char * from_user = strtok_r(NULL, ">", &syntax_check_mail_saveptr);
              if (from_user == NULL || from_user[0] == '\0') {
                send_formatted(fd, "553 Requested action not taken: mailbox name not allowed\r\n");
              } 
              else {
                send_formatted(fd, "250 OK\r\n");

                int mail_done = 0;
                int rcpt_received = 0;
                user_list_t recipient_list = create_user_list();
                while (mail_done == 0) {
                  nb_read_line(nb, recvbuf);
                  if (strchr(recvbuf, ' ')) {
                    command = strtok_r(recvbuf, " ", &command_saveptr);
                  }
                  else {
                    command = strtok_r(recvbuf, "\r\n", &command_saveptr);
                  }
                  if (strcasecmp(command, "RCPT") == 0) {
                    char * syntax_check_rcpt_saveptr;
                    char * syntax_check_rcpt = strtok_r(NULL, "\r\n", &command_saveptr);
                    if (strchr(syntax_check_rcpt, '<') && strchr(syntax_check_rcpt, '>')) {
                      if (strcasecmp(strtok_r(syntax_check_rcpt, "<", &syntax_check_rcpt_saveptr), "TO:") == 0) {
                        char * to_user = strtok_r(NULL, ">", &syntax_check_rcpt_saveptr);
                        if (is_valid_user(to_user, NULL) != 0) {
                          add_user_to_list(&recipient_list, to_user);
                          rcpt_received++;
                          send_formatted(fd, "250 OK\r\n");
                        }
                        else {
                          send_formatted(fd, "550 No such user here\r\n");
                        }
                      }
                      else {
                        send_formatted(fd, "501 Syntax error in parameters or arguments\r\n");
                      }
                    }
                    else {
                      send_formatted(fd, "501 Syntax error in parameters or arguments\r\n");
                    } 
                  }
                  else if (strcasecmp(command, "DATA") == 0) {
                    if (rcpt_received != 0) {
                      send_formatted(fd, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
                      char tmpfile[] = "mailtmp.XXXXXX";
                      int tmpfd = mkstemp(tmpfile);
                      nb_read_line(nb, recvbuf);
                      while (strcmp(recvbuf, ".\n") != 0) {
                        write(tmpfd, recvbuf, strlen(recvbuf));
                        nb_read_line(nb, recvbuf);
                      }
                      save_user_mail(tmpfile, recipient_list);
                      unlink(tmpfile);
                      close(tmpfd);
                      send_formatted(fd, "250 OK\r\n");
                      mail_done = 1;
                      break;
                    }
                    else {
                      send_formatted(fd, "503 Bad sequence of commands\r\n");
                    }
                  }
                  else if (strcasecmp(command, "NOOP") == 0) {
                    send_formatted(fd, "250 OK\r\n");
                  }
                  else if (strcasecmp(command, "VRFY") == 0) {
                    char * user = strtok_r(NULL, "\r\n", &command_saveptr);
                    if (is_valid_user(user, NULL) != 0) {
                      send_formatted(fd, "250 %s\r\n", user);
                    } 
                    else {
                      send_formatted(fd, "550 No such user here\r\n");
                    }
                  } 
                  else if (strcasecmp(command, "EHLO") == 0 || strcasecmp(command, "HELO") == 0) {
                    char * name = strtok_r(NULL, "\r\n", &command_saveptr);
                    if (name == NULL || name[0] == '\0') {
                      send_formatted(fd, "501 Syntax error in parameters or arguments (EHLO/HELO no paramaters or arguments)\r\n");
                    }
                    else {
                      destroy_user_list(recipient_list); 
                      send_formatted(fd, "250-%s greets %s\r\n", my_uname.__domainname, name);
                      received_greeting = 1;
                    }
                  }
                  else if (strcasecmp(command, "RSET") == 0) {
                    destroy_user_list(recipient_list);
                    send_formatted(fd, "250 OK\r\n");
                    mail_done = 1;
                  }
                  else if (strcasecmp(command, "QUIT") == 0) {
                    destroy_user_list(recipient_list);
                    mail_done = 1;
                    quit_received = 1;
                  }
                  else {
                    send_formatted(fd, "503 Bad sequence of commands\r\n");
                  }
                }
              }
            }
            else {
              send_formatted(fd, "501 Syntax error in parameters or arguments\r\n");
            }
          } 
          else {
            send_formatted(fd, "501 Syntax error in parameters or arguments\r\n");
          }
        } 
        else {
          send_formatted(fd, "503 Bad sequence of commands\r\n");
        }
      }
    }
  }

  send_formatted(fd, "221 %s Service closing transmission channel\r\n", my_uname.__domainname);  
  nb_destroy(nb);
  return;
}