#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <libconfig.h>
#include <pwd.h>
#include <mpd/client.h>

static struct mpd_connection *
setup_connection(void)
{
    struct mpd_connection *conn = mpd_connection_new("127.0.0.1", 6600, 0);
    if (conn == NULL) {
        fputs("Out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
        return 0;
/*
    if(options.password)
        send_password(options.password, conn);
*/
    return conn;
}

static bool
uri_has_scheme(const char *uri)
{
    return strstr(uri, "://") != NULL;
}

void
strip_trailing_slash(char *s)
{
    if (uri_has_scheme(s))
        /* strip slashes only if this string is relative to
           the music directory; absolute URLs are not, for
           sure */
        return;

    size_t len = strlen(s);

    if (len == 0)
        return;
    --len;

    if (s[len] == '/')
        s[len] = '\0';

    return;
}

void
print_entity_list(struct mpd_connection *c, enum mpd_entity_type filter_type)
{
    struct mpd_entity *entity;
    while ((entity = mpd_recv_entity(c)) != NULL) {
        const struct mpd_directory *dir;
        const struct mpd_song *song;
        const struct mpd_playlist *playlist;

        enum mpd_entity_type type = mpd_entity_get_type(entity);
        if (filter_type != MPD_ENTITY_TYPE_UNKNOWN &&
            type != filter_type)
            type = MPD_ENTITY_TYPE_UNKNOWN;

        switch (type) {
        case MPD_ENTITY_TYPE_UNKNOWN:
            break;

        case MPD_ENTITY_TYPE_DIRECTORY:
            dir = mpd_entity_get_directory(entity);
            printf("%s\n", mpd_directory_get_path(dir));
            break;

        case MPD_ENTITY_TYPE_SONG:
            song = mpd_entity_get_song(entity);
/*            if (options.custom_format) {
                pretty_print_song(song);
                puts("");
            } else*/
                printf("%s\n", mpd_song_get_uri(song));
            break;

        case MPD_ENTITY_TYPE_PLAYLIST:
            playlist = mpd_entity_get_playlist(entity);
            printf("%s\n", mpd_playlist_get_path(playlist));
            break;
        }

        mpd_entity_free(entity);
    }
}

static int
ls_entity(char *argv, struct mpd_connection *conn,
      enum mpd_entity_type type)
{
    const char *ls = "";

    if (argv)
        ls = argv;

    if (!mpd_send_list_meta(conn, ls))
        return -1;
    print_entity_list(conn, type);
//    my_finishCommand(conn);

    return 0;
}

int
cmd_ls(char *argv, struct mpd_connection *conn)
{
    strip_trailing_slash(argv);

    return ls_entity(argv, conn, MPD_ENTITY_TYPE_UNKNOWN);
}

int main(int argc, char **argv)
{
    int ret;

    struct mpd_connection *conn = setup_connection();

    if (mpd_connection_cmp_server_version(conn, 0, 16, 0) < 0)
        fprintf(stderr, "warning: MPD 0.16 required\n");

    ret = cmd_ls("radio", conn);

    mpd_connection_free(conn);

    return (ret >= 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
