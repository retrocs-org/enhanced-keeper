#include <mach-o/dyld.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char executable[PATH_MAX], resolved[PATH_MAX], game[PATH_MAX];
    uint32_t length = sizeof(executable);
    if (_NSGetExecutablePath(executable, &length) != 0 || !realpath(executable, resolved)) {
        perror("Cannot locate KeeperFX app");
        return 1;
    }
    char *separator = strrchr(resolved, '/');
    if (!separator) return 1;
    *separator = '\0';
    if (snprintf(game, sizeof(game), "%s/../Resources/game/keeper-enhanced", resolved) >= sizeof(game))
        return 1;
    char **arguments = calloc((size_t)argc + 1, sizeof(*arguments));
    if (!arguments) return 1;
    arguments[0] = game;
    for (int i = 1; i < argc; ++i) arguments[i] = argv[i];
    execv(game, arguments);
    perror("Cannot start KeeperFX");
    free(arguments);
    return 1;
}
