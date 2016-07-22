#include <stdio.h>

struct data {
    int age;
    struct data *next_data;
};

void test_recursion(struct data *_data) {
    struct data *my_data = _data;

    if (NULL != my_data->next_data) {
        my_data = my_data->next_data;
        test_recursion(my_data);

        printf("my_data age is %d\n", my_data->age);
    }


}


int main() {
    struct data my_data_1, my_data_2, my_data_3, my_data_4, my_data_5,my_data_6,my_data_7,my_data_8,my_data_9,my_data_10;
    my_data_1.age = 1;
    my_data_1.next_data = &my_data_2;
    my_data_2.age = 2;
    my_data_2.next_data = &my_data_3;
    my_data_3.age = 3;
    my_data_3.next_data = &my_data_4;
    my_data_4.age = 4;
    my_data_4.next_data = &my_data_5;
    my_data_5.age = 5;
    my_data_5.next_data = &my_data_6;
    my_data_6.age = 6;
    my_data_6.next_data = &my_data_7;
    my_data_7.age = 7;
    my_data_7.next_data = &my_data_8;
    my_data_8.age = 8;
    my_data_8.next_data = &my_data_9;
    my_data_9.age = 9;
    my_data_9.next_data = &my_data_10;
    my_data_10.age = 10;
    my_data_10.next_data = NULL;


    test_recursion(&my_data_1);
    return 0;
}
