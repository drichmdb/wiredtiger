/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "test_util.h"

#include <sys/types.h>
#include <sys/wait.h>

/*
 * Command-line arguments.
 */
#define SHARED_PARSE_OPTIONS "h:p"

static char home[PATH_MAX]; /* Program working dir */
static TEST_OPTS *opts, _opts;

extern int __wt_optind;
extern char *__wt_optarg;

static void usage(void) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));

/*
 * Configuration.
 */
#define ENV_CONFIG                                            \
    "cache_size=20M,create,"                                  \
    "debug_mode=(table_logging=true,checkpoint_retention=5)," \
    "eviction_updates_target=20,eviction_updates_trigger=90," \
    "log=(enabled,file_max=10M,remove=true),session_max=100," \
    "statistics=(all),statistics_log=(wait=1,json,on_close)"

#define BACKUP_BASE "backup."
#define CHECK_DIR "check"
#define NUM_BACKUPS 3
#define TABLE_CONFIG "key_format=S,value_format=S,log=(enabled=false)"
#define TABLE_NAME "table"
#define TABLE_URI ("table:" TABLE_NAME)

/*
 * Other constants.
 */
#define EXPECT_ABORT "expect_abort"

/*
 * handler_sigchld --
 *     Signal handler to catch if the child died.
 */
static void
handler_sigchld(int sig)
{
    pid_t pid;

    pid = wait(NULL);
    WT_UNUSED(sig);

    if (testutil_exists(NULL, EXPECT_ABORT))
        return;

    /*
     * The core file will indicate why the child exited. Choose EINVAL here.
     */
    testutil_die(EINVAL, "Child process %" PRIu64 " abnormally exited", (uint64_t)pid);
}

/*
 * populate_table --
 *     Populate the table with random data.
 */
static void
populate_table(WT_SESSION *session, const char *uri, uint32_t prefix, uint64_t num_keys)
{
    WT_CURSOR *cursor;
    uint64_t i;
    uint32_t k;
    char key[32], value[32];

    testutil_check(session->open_cursor(session, uri, NULL, NULL, &cursor));
    for (i = 0; i < num_keys; i++) {
        k = __wt_random(&opts->data_rnd);
        testutil_snprintf(key, sizeof(key), "%010" PRIu32 ":%010" PRIu32, prefix, k);
        testutil_snprintf(value, sizeof(value), "%010" PRIu32, ~k);
        cursor->set_key(cursor, key);
        cursor->set_value(cursor, value);
        testutil_check(cursor->insert(cursor));
    }
    testutil_check(cursor->close(cursor));
}

/*
 * verify_backup --
 *     Verify the backup's consistency.
 */
static void
verify_backup(const char *backup_home)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_SESSION *session;
    uint32_t k, v;
    char *key, *value;

    /* Copy the backup. */
    testutil_copy(backup_home, CHECK_DIR);

    /* Open the backup. */
    testutil_wiredtiger_open(opts, CHECK_DIR, ENV_CONFIG, NULL, &conn, true, false);
    testutil_check(conn->open_session(conn, NULL, NULL, &session));

    /* Verify self-consistency. */
    testutil_check(session->open_cursor(session, TABLE_URI, NULL, NULL, &cursor));
    while ((ret = cursor->next(cursor)) == 0) {
        testutil_check(cursor->get_key(cursor, &key));
        testutil_check(cursor->get_value(cursor, &value));
        testutil_assert(strlen(key) > 11);
        k = (uint32_t)atoll(key + 11);
        v = (uint32_t)atoll(value);
        testutil_assert(k == ~v);
    }
    testutil_assert(ret == WT_NOTFOUND);
    testutil_check(cursor->close(cursor));

    /* Cleanup. */
    testutil_check(session->close(session, NULL));
    testutil_check(conn->close(conn, NULL));
}

/*
 * run_test --
 *     Run the test.
 */
static void
run_test(void)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_SESSION *session;
    pid_t parent_pid, pid;
    int i, id, status;
    char backup_home[64], backup_id[32], src_backup_home[64], src_backup_id[32], *str;

    parent_pid = getpid();
    testutil_assert_errno((pid = fork()) >= 0);

    if (pid == 0) { /* Child. */
        testutil_recreate_dir(WT_HOME_DIR);
        testutil_wiredtiger_open(opts, WT_HOME_DIR, ENV_CONFIG, NULL, &conn, false, false);
        testutil_check(conn->open_session(conn, NULL, NULL, &session));
        testutil_check(session->create(session, TABLE_URI, TABLE_CONFIG));

        /* Do some work, while creating checkpoints and doing backups. */
        for (i = 0; i < NUM_BACKUPS; i++) {
            populate_table(session, TABLE_URI, (uint32_t)i, 100 * WT_THOUSAND);
            testutil_check(session->checkpoint(session, NULL));
            populate_table(session, TABLE_URI, (uint32_t)i + 1, 100 * WT_THOUSAND);
            testutil_check(session->checkpoint(session, NULL));
            populate_table(session, TABLE_URI, (uint32_t)i + 2, 100 * WT_THOUSAND);

            testutil_snprintf(backup_home, sizeof(backup_home), BACKUP_BASE "%d", i);
            testutil_snprintf(backup_id, sizeof(backup_id), "ID%d", i);
            if (i == 0) {
                printf("Create full backup %d\n", i);
                testutil_backup_create_full(
                  conn, WT_HOME_DIR, backup_home, backup_id, true, 32, NULL);
            } else {
                printf("Create incremental backup %d from %d\n", i, i - 1);
                testutil_snprintf(
                  src_backup_home, sizeof(src_backup_home), BACKUP_BASE "%d", i - 1);
                testutil_snprintf(src_backup_id, sizeof(src_backup_id), "ID%d", i - 1);
                testutil_backup_create_incremental(conn, WT_HOME_DIR, backup_home, backup_id,
                  src_backup_home, src_backup_id, false /* verbose */, NULL, NULL, NULL);
            }
        }

        /* Die before finishing the next checkpoint. */
        printf("Setting the failpoint...\n");
        testutil_check(
          session->reconfigure(session, "debug=(checkpoint_fail_before_turtle_update=true)"));
        testutil_sentinel(NULL, EXPECT_ABORT);
        testutil_check(session->checkpoint(session, NULL));
        testutil_remove(EXPECT_ABORT);

        /* We should die before we get here. */
        testutil_die(ENOTRECOVERABLE, "The child process was supposed be dead by now!");
    }

    /* Parent. */

    /* Wait for the child to die. */
    testutil_assert(waitpid(pid, &status, 0) > 0);
    printf("-- crash --\n");

    /* Save the database directory. */
    testutil_copy(WT_HOME_DIR, "save");

    /* Reopen the database and find available backup IDs. */
    testutil_wiredtiger_open(opts, WT_HOME_DIR, ENV_CONFIG, NULL, &conn, false, false);
    testutil_check(conn->open_session(conn, NULL, NULL, &session));

    ret = session->open_cursor(session, "backup:query_id", NULL, NULL, &cursor);
    testutil_check(ret);
    id = -1;
    while ((ret = cursor->next(cursor)) == 0) {
        testutil_check(cursor->get_key(cursor, &str));
        testutil_assert(strncmp(str, "ID", 2) == 0);
        i = atoi(str + 2);
        id = id < 0 ? i : WT_MIN(id, i);
        printf("Found backup %d\n", i);
    }
    testutil_assert(ret == WT_NOTFOUND);
    testutil_check(cursor->close(cursor));
    testutil_assert(id >= 0);

    /* Do more regular work. */
    populate_table(session, TABLE_URI, NUM_BACKUPS, 100 * WT_THOUSAND);

    /* Create an incremental backup. */
    testutil_snprintf(backup_home, sizeof(backup_home), BACKUP_BASE "%d", NUM_BACKUPS);
    testutil_snprintf(backup_id, sizeof(backup_id), "ID%d", NUM_BACKUPS);
    testutil_snprintf(src_backup_home, sizeof(src_backup_home), BACKUP_BASE "%d", id);
    testutil_snprintf(src_backup_id, sizeof(src_backup_id), "ID%d", id);

    printf("Create incremental backup %d from %d\n", NUM_BACKUPS, id);
    testutil_backup_create_incremental(conn, WT_HOME_DIR, backup_home, backup_id, src_backup_home,
      src_backup_id, false /* verbose */, NULL, NULL, NULL);

    /* Cleanup. */
    testutil_check(session->close(session, NULL));
    testutil_check(conn->close(conn, NULL));

    /* Verify the backup. */
    printf("Verify backup %d\n", NUM_BACKUPS);
    verify_backup(backup_home);
}

/*
 * usage --
 *     Print usage help for the program.
 */
static void
usage(void)
{
    fprintf(stderr, "usage: %s%s\n", progname, opts->usage);
    exit(EXIT_FAILURE);
}

/*
 * main --
 *     The entry point for the test.
 */
int
main(int argc, char *argv[])
{
    struct sigaction sa;
    int ch;
    char start_cwd[PATH_MAX];

    (void)testutil_set_progname(argv);

    /* Automatically flush after each newline, so that we don't miss any messages if we crash. */
    __wt_stream_set_line_buffer(stderr);
    __wt_stream_set_line_buffer(stdout);

    opts = &_opts;
    memset(opts, 0, sizeof(*opts));

    /* Parse the command-line arguments. */
    testutil_parse_begin_opt(argc, argv, SHARED_PARSE_OPTIONS, opts);
    while ((ch = __wt_getopt(progname, argc, argv, SHARED_PARSE_OPTIONS)) != EOF)
        switch (ch) {
        default:
            if (testutil_parse_single_opt(opts, ch) != 0)
                usage();
        }
    argc -= __wt_optind;
    if (argc != 0)
        usage();

    testutil_parse_end_opt(opts);
    testutil_work_dir_from_path(home, sizeof(home), opts->home);

    /* Create the test directory. */
    testutil_recreate_dir(home);
    testutil_assert_errno(getcwd(start_cwd, sizeof(start_cwd)) != NULL);
    testutil_assert_errno(chdir(home) == 0);

    /* Configure the child death handling. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_sigchld;
    testutil_assert_errno(sigaction(SIGCHLD, &sa, NULL) == 0);

    /* Run the test. */
    run_test();

    /*
     * Clean up.
     */
    testutil_assert_errno(chdir(start_cwd) == 0);

    /* Delete the work directory. */
    if (!opts->preserve)
        testutil_remove(home);

    testutil_cleanup(opts);
    return (EXIT_SUCCESS);
}
