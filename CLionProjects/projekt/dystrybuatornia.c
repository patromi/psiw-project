#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

struct zamowienie {
    long mtype; // odbiorca  1-dystrybuatornia, 2-kurier
    int id; // nadawca 1 - dystrybuatornia, 2 - magazyn1, 3 - magazyn2, 4 - magazyn3
    int magazyn;
    char command[6];
    int A;
    int B;
    int C;
};

int main(int argc, const char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s <klucz> <liczba_zamówień> <max_A_per_zam> <max_B_per_zam> <max_C_per_zam>\n", argv[0]);
        return 1;
    }
    if (atoi(argv[1]) < 1) {
        printf("Klucz musi być liczbą dodatnią\n");
        return 1;
    }


    key_t klucz = atoi(argv[1]);
    int liczba_zamowien = atoi(argv[2]);
    int max_A_per_zam = atoi(argv[3]);
    int max_B_per_zam = atoi(argv[4]);
    int max_C_per_zam = atoi(argv[5]);
    if (liczba_zamowien < 1 || max_A_per_zam < 1 || max_B_per_zam < 1 || max_C_per_zam < 1) {
        printf("Liczba zamówień oraz ilość produktów w zamówieniu musi być większa od 0\n");
        return 1;
    }


    int kolejka = msgget(klucz, 0666 | IPC_CREAT);
    if (kolejka == -1) {
        perror("msgget");
        return 1;
    }

    srand(time(NULL)); // Inicjalizacja generatora liczb losowych

    for (int i = 0; i < liczba_zamowien; i++) {
        usleep(100); // Czekaj 0.5 sekundy
        struct zamowienie zam;
        zam.mtype = 2; // Dystrybuatornia
        zam.A = rand() % (max_A_per_zam + 1);
        zam.B = rand() % (max_B_per_zam + 1);
        zam.C = rand() % (max_C_per_zam + 1);
        zam.id = 1;
        zam.magazyn = 0;
        strcpy(zam.command, "z");

        if (msgsnd(kolejka, &zam, sizeof(zam) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            return 1;
        } else {
            printf("Zlecono zamówienie %d: %d xA, %d xB, %d xC\n", i + 1, zam.A, zam.B, zam.C);
        }
    }
    int gold = 0;
    int counter = 0;
    while (1) {
        usleep(500000);
        struct zamowienie zam;
        int status = msgrcv(kolejka, &zam, sizeof(zam) - sizeof(long), 1, 0);
        if (status == -1) {
            perror("msgrcv");
            continue;
        }

        if (strcmp(zam.command, "end") == 0) {
            printf("Kurier numer %d z magazynu %d zakończył pracę\n",zam.id, zam.magazyn);

            counter++;
        }

        if (strcmp(zam.command, "ready") == 0) {
            printf("Kurier o id %d z magazynu %d dostarczył zamówienie\n", zam.id, zam.magazyn);
            gold = gold + zam.A;
        }

        if (counter == 9) {
            printf("Wszystkie magazyny zakończyły pracę, zamknięcie dystrybuatorni\n");
            if (msgctl(kolejka, IPC_RMID, NULL) == -1) {
                perror("msgctl");
            }
            printf("Zarobek dystrybuatorni: %d\n", gold);
            exit(0);
            break;
        }
    }
    return 0;
}