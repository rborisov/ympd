int
cmd_list(int argc, char **argv, struct mpd_connection *conn)
{
    enum mpd_tag_type type = get_search_type(argv[0]);
    if (type == MPD_TAG_UNKNOWN)
        return -1;

    --argc;
    ++argv;

    mpd_search_db_tags(conn, type);

    if (argc > 0 && !add_constraints(argc, argv, conn))
        return -1;

    if (!mpd_search_commit(conn))
        printErrorAndExit(conn);

    struct mpd_pair *pair;
    while ((pair = mpd_recv_pair_tag(conn, type)) != NULL) {
        printf("%s\n", charset_from_utf8(pair->value));
        mpd_return_pair(conn, pair);
    }

    my_finishCommand(conn);
    return 0;
}
