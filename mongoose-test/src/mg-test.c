// Copyright (c) 2004-2012 Sergey Lyubka
// This file is a part of mongoose project, http://github.com/valenok/mongoose

#include <stdio.h>
#include <string.h>
#include "mongoose.h"

static int index_html(struct mg_connection *conn) {
  const char *data;
  int data_len;
  char var_name[100], file_name[100];

  mg_printf_data(conn, "%s",
                 "<html><body>Upload example."
                 "<form method=\"POST\" action=\"/handle_post_request\" "
                 "  enctype=\"multipart/form-data\">"
                 "<input type=\"file\" name=\"file\" /> <br/>"
                 "<input type=\"submit\" value=\"Upload\" />"
                 "</form>");

  if (mg_parse_multipart(conn->content, conn->content_len,
                         var_name, sizeof(var_name),
                         file_name, sizeof(file_name),
                         &data, &data_len) > 0) {

    mg_printf_data(conn, "%s", "Uploaded file:<pre>");
    mg_send_data(conn, data, data_len);
    mg_printf_data(conn, "%s", "/pre>");
  }

  mg_printf_data(conn, "%s", "</body></html>");

  return MG_REQUEST_PROCESSED;
}

int main(void) {
  struct mg_server *server;
  FILE *fd;
  int i = 0;
  int buf[10000], tmp;

  // Create and configure the server
  server = mg_create_server(NULL);
  mg_set_option(server, "listening_port", "8080");
  mg_set_request_handler(server, index_html);

  // Serve request. Hit Ctrl-C to terminate the program
  printf("Starting on port %s\n", mg_get_option(server, "listening_port"));
  fd = fopen("art.jpg", "r");
  if(!fd)
      printf("Failed open file\n");
  else
  {
      while ((tmp = fgetc(fd)) != EOF)
      {
          printf(" %#04x, ", tmp);
          buf[i] = tmp;
          i++;
      }
      printf("%i i %i\n", fd, i);
      fclose(fd);
//      mg_send_header(c, "Content-Type", "image/jpeg");
//      mg_send_data(c, buf, i);

//      return MG_REQUEST_PROCESSED;
  }

  for (;;) {
    mg_poll_server(server, 1000);
  }

  // Cleanup, and free server instance
  mg_destroy_server(&server);

  return 0;
}
