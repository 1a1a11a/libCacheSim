//
// Created by Haocheng on 01/14/25.
//

#include "../libCacheSim/utils/include/mysys.h"
#include "common.h"

bool directory_exists(const char *path) {
    struct stat info;
    return (stat(path, &info) == 0 && (info.st_mode & S_IFDIR));
}

void test_create_dir(gconstpointer user_data) {
    char base_dir[] = "/tmp/gtest_dir_test";

    if (directory_exists(base_dir)) {
        if(system("rm -rf /tmp/gtest_dir_test") != 0) {
            perror("Failed to delete file");
        }

    }

    // case 1: single dir
    char single_dir[1024];
    snprintf(single_dir, sizeof(single_dir), "%s/single", base_dir);
    create_dir(single_dir);
    g_assert_true(directory_exists(single_dir));

    // case 2: multi dir
    char multi_dir[1024];
    snprintf(multi_dir, sizeof(multi_dir), "%s/multi/level/directory", base_dir);
    create_dir(multi_dir);
    g_assert_true(directory_exists(multi_dir));

    // case 3: dir exists
    create_dir(single_dir);
    g_assert_true(directory_exists(single_dir));

    // case 4: tail /
    char slash_dir[1024];
    snprintf(slash_dir, sizeof(slash_dir), "%s/with_slash/", base_dir);
    create_dir(slash_dir);
    g_assert_true(directory_exists(slash_dir));

    // printf("All create_dir tests passed!\n");
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_data_func("/test_create_dir", NULL, test_create_dir);
    return g_test_run();
}
