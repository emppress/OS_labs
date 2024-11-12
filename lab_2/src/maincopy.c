#include "stdio.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>

int success_events = 0;
pthread_mutex_t m;

typedef struct Cards
{
    int suit;
    int ranks;
} Cards;

typedef struct arg_struct
{
    Cards *cards_arr;
    int count_tests;
} arg_t;

void print(const char *text)
{
    if (!text)
        return;

    if (write(STDOUT_FILENO, text, strlen(text)) == -1)
        exit(EXIT_FAILURE);
}

void *check_probability(void *__args)
{
    arg_t *args = (arg_t *)__args;
    int count_succes = 0;
    int idx_1, idx_2;
    srand((unsigned)time(NULL));
    for (int i = 0; i < args->count_tests; ++i)
    {
        idx_1 = rand() % 52;
        do
        {
            idx_2 = rand() % 52;
        } while (idx_2 == idx_1);
        if (args->cards_arr[idx_1].suit == args->cards_arr[idx_2].suit)
            count_succes++;
    }
    if (pthread_mutex_lock(&m))
    {
        print("Pthread_mutex_lock error\n");
        exit(EXIT_FAILURE);
    }
    success_events += count_succes;
    if (pthread_mutex_unlock(&m))
    {
        print("Pthread_mutex_unlock error\n");
        exit(EXIT_FAILURE);
    }
    return NULL;
}

void create_cards_arr(Cards *cards_arr)
{
    int idx = 0;
    if (!cards_arr)
        return;

    for (int i = 1; i < 14; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            cards_arr[idx].ranks = i;
            cards_arr[idx++].suit = j;
        }
    }
}

int main(int argc, char **argv)
{
    int max_count_treads, count_rounds, remainder;
    if (argc != 3)
    {
        print("Input error. Enter <program_name><max_count_treads><count_rounds>\n");
        exit(EXIT_FAILURE);
    }
    max_count_treads = atoi(argv[1]);
    count_rounds = atoi(argv[2]);
    if (pthread_mutex_init(&m, NULL))
    {
        print("Pthread_mutex_create error\n");
        exit(EXIT_FAILURE);
    }

    pthread_t treads[max_count_treads];
    Cards cards_arr[52];
    create_cards_arr(cards_arr);

    remainder = count_rounds % max_count_treads;
    arg_t args = {.cards_arr = cards_arr, .count_tests = count_rounds / max_count_treads};

    if (remainder)
    {
        args.count_tests += remainder;
        if (pthread_create(&treads[0], NULL, check_probability, (void *)(&args)))
        {
            print("Pthread_create error\n");
            exit(EXIT_FAILURE);
        }
        args.count_tests -= remainder;
    }

    for (int i = (remainder) ? 1 : 0; i < max_count_treads; ++i)
    {
        if (pthread_create(&treads[i], NULL, check_probability, (void *)(&args)))
        {
            print("Pthread_create error\n");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < max_count_treads; ++i)
    {
        if (pthread_join(treads[i], NULL))
        {
            print("Pthread_join error\n");
            exit(EXIT_FAILURE);
        }
    }
    char result[100];
    if (pthread_mutex_destroy(&m))
    {
        print("Pthread_mutex_destroy error\n");
        exit(EXIT_FAILURE);
    }
    sprintf(result, "%lf%%\n", (double)success_events / count_rounds * 100);
    print(result);
}