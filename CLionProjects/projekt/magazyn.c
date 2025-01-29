#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_FORKS 3

struct config {
    int count_A, prize_A;
    int count_B, prize_B;
    int count_C, prize_C;
    int id;
};

struct zamowienie {
    long mtype;
    int id, magazyn;
    char command[6];
    int A, B, C;
};

struct config load_config(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    char buf[512];
    struct config conf;
    int bytes = read(fd, buf, sizeof(buf) - 1);
    if (bytes == -1) {
        perror("read");
        close(fd);
        exit(1);
    }
    buf[bytes] = '\0';
    close(fd);

    sscanf(buf, "%d %d\n%d %d\n%d %d", &conf.count_A, &conf.prize_A, &conf.count_B, &conf.prize_B, &conf.count_C, &conf.prize_C);
    sscanf(strrchr(path, 'm') + 1, "%d", &conf.id);

    return conf;
}

void run_kurier(int kurier_id, key_t global_key_id, key_t magazyn_key_id, int magazyn_id, struct config config) {
    while (1) {
        struct zamowienie zam;
        printf("Kurier %d czeka na zamówienie...\n", kurier_id);

        int global_queue = msgget(global_key_id, 0666 | IPC_CREAT);
        if (global_queue == -1) {
            perror("msgget (global_queue)");
            exit(1);
        }

        int status = msgrcv(global_queue, &zam, sizeof(zam) - sizeof(long), 2, 0);
        if (status == -1) {
            perror("msgrcv (global_queue)");
            exit(1);
        }

        printf("Kurier %d odebrał zamówienie: %d xA, %d xB, %d xC\n", kurier_id, zam.A, zam.B, zam.C);

        zam.mtype = 3;
        zam.id = kurier_id;
        zam.magazyn = magazyn_id;
        strcpy(zam.command, "z");

        int magazyn_queue = msgget(magazyn_key_id, 0666 | IPC_CREAT);
        if (msgsnd(magazyn_queue, &zam, sizeof(zam) - sizeof(long), 0) == -1) {
            perror("msgsnd (magazyn_queue)");
            exit(1);
        }
        printf("Kurier %d wysłał zamówienie do magazynu %d\n", kurier_id, magazyn_id);

        status = msgrcv(magazyn_queue, &zam, sizeof(zam) - sizeof(long), 2, 0);
        if (status == -1) {
            perror("msgrcv (magazyn_queue)");
            exit(1);
        }

        if (strcmp(zam.command, "out") == 0) {
            printf("Kurier %d: brak produktów, kończy pracę.\n", kurier_id);
            zam.mtype = 1;
            zam.id = kurier_id;
            strcpy(zam.command, "end");
            msgsnd(global_queue, &zam, sizeof(zam) - sizeof(long), 0);
            exit(0);
        }

        printf("Kurier %d odebrał zamówienie, dostarcza do klienta\n", kurier_id);
        zam.mtype = 1;
        zam.id = kurier_id;
        zam.magazyn = magazyn_id;
        zam.A = zam.A * config.prize_A + zam.B * config.prize_B + zam.C * config.prize_C;
        strcpy(zam.command, "ready");
        if (msgsnd(global_queue, &zam, sizeof(zam) - sizeof(long), 0) == -1) {
            perror("msgsnd (global_queue)");
            exit(1);
        }
        printf("Kurier %d dostarczył zamówienie\n", kurier_id);
    }
}

void run_magazyn(struct config c, key_t global_key) {
    key_t magazyn_key = global_key + c.id;
    int magazyn_queue = msgget(magazyn_key, 0666 | IPC_CREAT);
    if (magazyn_queue == -1) {
        perror("msgget (magazyn_queue)");
        exit(1);
    }

    for (int i = 0; i < MAX_FORKS; ++i) {
        if (fork() == 0) {
            run_kurier(i, global_key, magazyn_key, c.id, c);
            exit(0);
        }
    }

    while (1) {
        struct zamowienie zam;
        printf("Magazyn %d czeka na prośbę od kuriera...\n", c.id);
        int status = msgrcv(magazyn_queue, &zam, sizeof(zam) - sizeof(long), 3, 0);
        if (status == -1) {
            perror("msgrcv (magazyn_queue)");
            continue;
        }

        printf("Magazyn %d: zamówienie od kuriera %d - %d xA, %d xB, %d xC\n", c.id, zam.id, zam.A, zam.B, zam.C);

        if (c.count_A >= zam.A && c.count_B >= zam.B && c.count_C >= zam.C) {
            c.count_A -= zam.A;
            c.count_B -= zam.B;
            c.count_C -= zam.C;
            zam.mtype = 2;
            strcpy(zam.command, "ready");
        } else {
            zam.mtype = 2;
            strcpy(zam.command, "out");
        }

        if (msgsnd(magazyn_queue, &zam, sizeof(zam) - sizeof(long), 0) == -1) {
            perror("msgsnd (magazyn_queue)");
            exit(1);
        }
        printf("Magazyn %d: %s dla kuriera %d\n", c.id, strcmp(zam.command, "ready") == 0 ? "zamówienie gotowe" : "brak produktów", zam.id);
    }
}

int main(int argc, const char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <config> <key>\n", argv[0]);
        return 1;
    }

    key_t key = atoi(argv[2]);
    struct config c = load_config(argv[1]);

    printf("Magazyn %d uruchomiony: %d xA, %d xB, %d xC\n", c.id, c.count_A, c.count_B, c.count_C);

    run_magazyn(c, key);

    wait(NULL);
    return 0;
}