#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <zconf.h>
#include <cerrno>

using namespace cv;
using namespace std;
#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
//图片像素结构体
struct picture_pix {
    int B;
    int G;
    int R;

    int location_row; //该像素处于图像的第几行
    int start_offset; //该像素距离本行开头的距离
    bool is_word_pix; //是否是字体像素
    struct picture_pix *next_pix; //下一个像素
    struct picture_pix *prev_pix; //前一个像素

};


//把openc的矩阵转成咱们的结构体
int transform_to_mstrcut(Mat &I, struct picture_pix **pic_pix);

//todo，判断当前像素是否是字体像素，以后可能要根据前后的连续像素做判断，先封装成函数
int check_is_word_pix(struct picture_pix *backgound_pix, struct picture_pix *current_pix);

void *thread_check_top_big_bottom(void *arg);

void check_top_big_bottom(bool *top_big_bottom, int I_rows, int I_cols, struct picture_pix *first_pix,
                          pthread_cond_t *check_top_big_bottom_cond, int *tourch_min_center_calculate_num_count);

int check_hole(struct picture_pix *first_pix, Mat *I);

void get_pix_pointer(int row, int col, struct picture_pix *first_pix, Mat *I, struct picture_pix **find_pix);

struct thread_top_big_bottom_check_strcut {
    int start_index;
    int end_index;
    int *distance_array[];
    bool *top_big_bottom;
    //条件，用于结果完成，通知
    pthread_cond_t *check_top_big_bottom_cond;
    int *tourch_min_center_calculate_num_count;
};
//二值化计算像素数量最少值，超过这阀值做中间计算才有价值。
int min_center_calculate_num = 10;

int main(int argc, char **argv) {

    if (argc < 2) {
        cout << "Not enough parameters" << endl;
        return -1;
    }

    Mat I, J;

    I = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    //cout << "E = " << endl << " " << I << endl << endl;


    if (!I.data) {
        cout << "The image" << argv[1] << " could not be loaded." << endl;
        return -1;
    }

    struct picture_pix *first_pix;
    transform_to_mstrcut(I, &first_pix);

    int hole_num = check_hole(first_pix, &I);


    bool top_big_bottom = true;
    //触碰到最小阀值的次数，必须触碰两次
    int *tourch_min_center_calculate_num_count = (int *) malloc(sizeof(int));
    *tourch_min_center_calculate_num_count = 0;
    pthread_mutex_t check_top_big_bottom_cond_mtx = PTHREAD_MUTEX_INITIALIZER;
    //新建一个条件锁，等待被触发
    pthread_cond_t check_top_big_bottom_cond = PTHREAD_COND_INITIALIZER;
    //加锁
    pthread_mutex_lock(&check_top_big_bottom_cond_mtx);

    check_top_big_bottom(&top_big_bottom, I.rows, I.cols, first_pix, &check_top_big_bottom_cond,
                         tourch_min_center_calculate_num_count);

    //结果校验while，pthread_cond_wait可能被意外唤醒
    while (*tourch_min_center_calculate_num_count < 2) {
        //阻塞等待子线程通知，触碰两次最小阀值
        pthread_cond_wait(&check_top_big_bottom_cond, &check_top_big_bottom_cond_mtx);
    }

    if (hole_num == 1 && top_big_bottom) {
        printf("输入的图片是A\n");
    } else {
        printf("输入的图片不是A\n");
    }

    printf("OCR 检测结束\n");
    free(tourch_min_center_calculate_num_count);

    //todo,树状矩阵内存没有free
    return 0;

}

//检测字母是否存在洞
int check_hole(struct picture_pix *first_pix, Mat *I) {


    int hole_edge_distance = 3; //洞边缘像素的最大差距，超过这个值就是有缺口。
    //判断从上到下的行，是否满足边缘脚点，2->4，4,4->2，
    struct picture_pix *temp_pix = first_pix;

    //先算出每一行有多少个边缘脚点。
    int row_edge_pix_number[I->rows];
    memset(row_edge_pix_number, 0, sizeof(row_edge_pix_number));

    for (int i = 1; i <= I->rows * I->cols; ++i) {
        if (true == temp_pix->is_word_pix) {

            //如果头尾是字像素，边缘脚点就++
            if (0 == temp_pix->start_offset || temp_pix->start_offset == (I->cols - 1)) {
                row_edge_pix_number[temp_pix->location_row - 1]++;
            }

            //前后元素不是字体像素，边缘脚点就++
            if (false == temp_pix->prev_pix->is_word_pix &&
                temp_pix->prev_pix->location_row == temp_pix->location_row) {
                row_edge_pix_number[temp_pix->location_row - 1]++;
            }

            if (false == temp_pix->next_pix->is_word_pix &&
                temp_pix->next_pix->location_row == temp_pix->location_row) {
                row_edge_pix_number[temp_pix->location_row - 1]++;
            }

        }
        temp_pix = temp_pix->prev_pix;
    }

    struct perhaps_hole {
        int start_row;
        int end_row;
        bool is_real_hole;    //是否是真实的洞
        struct perhaps_hole *next_perhasps_hole;
    };

    struct perhaps_hole *perhapsHole = (struct perhaps_hole *) malloc(sizeof(struct perhaps_hole));
    perhapsHole->is_real_hole = true;
    //第一个头指针要保留起来。
    struct perhaps_hole *firstPerhapsHole = perhapsHole;
    //可能洞的数量
    int perhaps_hole_number = 0;
    //循环，找出哪个脚点最可能形成洞
    for (int j = 0; j < I->rows; ++j) {
        if (4 == row_edge_pix_number[j]) {
            //如果前面一行的脚点为2，很可能前一行就是洞顶
            if (2 == row_edge_pix_number[j - 1]) {
                //循环找出下面的一个4的行，就是洞底
                for (int i = (j + 1); i < I->rows; ++i) {
                    //找到洞底
                    if (2 == row_edge_pix_number[i]) {

                        perhapsHole->start_row = j + 1;
                        perhapsHole->end_row = i;
                        struct perhaps_hole *nextPerhapsHole = (struct perhaps_hole *) malloc(
                                sizeof(struct perhaps_hole));
                        perhapsHole->next_perhasps_hole = nextPerhapsHole;
                        perhapsHole = nextPerhapsHole;
                        perhaps_hole_number++;

                        j = i;
                        break;
                    }
                }
            }
        }
    }

    perhapsHole = firstPerhapsHole;
    for (int l = 0; l < perhaps_hole_number; ++l) {

        //先把左边洞边缘点距离行首元素的距离用int[] 存起来，
        int left_edge_distance[perhapsHole->end_row - perhapsHole->start_row + 1];
        int left_edge_distance_index = 0;
        struct picture_pix *start_pix;

        //找到开始行的首像素再循环，逻辑清晰点
        get_pix_pointer(perhapsHole->start_row, 1, first_pix, I, &start_pix);
        temp_pix = start_pix;

        for (int i = 0; i < I->rows * I->cols; ++i) {
            //printf("i is is %d\n",i);
            //同一行后一个元素不是字体像素，如果是洞顶跟洞底，找左边第一个脚
            if (true == temp_pix->is_word_pix && true != temp_pix->next_pix->is_word_pix &&
                temp_pix->next_pix->location_row == temp_pix->location_row) {
                //找到左边边缘点
                left_edge_distance[left_edge_distance_index] = temp_pix->next_pix->start_offset;
                if (left_edge_distance_index == (perhapsHole->end_row - perhapsHole->start_row)) {
                    break;
                } else {
                    left_edge_distance_index++;
                }

                //立即跳到下一行
                int current_row = temp_pix->location_row;
                for (int k = 0; k < I->cols; ++k) {
                    temp_pix = temp_pix->next_pix;

                    //已经跳到下一行
                    if (current_row != temp_pix->location_row) {
                        //返回
                        break;
                    } else {
                        //注意，这里不是下一行 才加。
                        i++;
                    }
                }
            } else {
                temp_pix = temp_pix->next_pix;
            }
        }

        //先把左边洞边缘点距离行首元素的距离用int[] 存起来，
        int right_edge_distance[perhapsHole->end_row - perhapsHole->start_row + 1];
        int right_edge_distance_index = 0;
        temp_pix = start_pix;

        //I->rows * I->cols 这里I->rows * I->cols没有啥特别意义，就是用一个比较大的数子来循环。保证可以循环完洞边缘点，里面有break跳出
        for (int i = 0; i < I->rows * I->cols; ++i) {
            //printf("i is is %d\n",i);
            //同一行后一个元素不是字体像素
            if (true == temp_pix->is_word_pix && true != temp_pix->next_pix->is_word_pix &&
                temp_pix->next_pix->location_row == temp_pix->location_row) {

                //先保留左边边缘脚点
                //struct picture_pix * left_edge_pix =  temp_pix->next_pix;

                temp_pix = temp_pix->next_pix;

                //再次循环，找到右边边缘点
                for (int j = 0; j < I->cols; ++j) {
                    //如果后一个元素不是字体像素点，就是已经找到右边的边缘点
                    if (true == temp_pix->next_pix->is_word_pix &&
                        temp_pix->next_pix->location_row == temp_pix->location_row) {
                        right_edge_distance[right_edge_distance_index] = temp_pix->start_offset;

                        if (right_edge_distance_index == (perhapsHole->end_row - perhapsHole->start_row)) {
                            //遍历完洞，立即跳出，可以增大i的值跳出
                            i = I->rows * I->cols;
                            break;
                        } else {
                            right_edge_distance_index++;
                            //找到一个右边边缘点立即跳到下一行，同时break出循环
                            //立即跳到下一行
                            int current_row = temp_pix->location_row;
                            for (int n = 0; n < I->cols; ++n) {
                                temp_pix = temp_pix->next_pix;

                                //已经跳到下一行
                                if (current_row != temp_pix->location_row) {
                                    //同时要跳出j循环
                                    j = I->cols;
                                    //返回
                                    break;
                                } else {
                                    //注意，这里不是下一行 才加。
                                    //i++;
                                }
                            }
                        }

                    } else {
                        //如果temp本身是尾部元素，temp不可能是尾部像素，如果是就无法闭合
                        temp_pix = temp_pix->next_pix;
                    }
                }


            } else {
                temp_pix = temp_pix->next_pix;
            }
        }

        //判断洞边缘是不是连续分布的，
        for (int k = 0; k < (perhapsHole->end_row - perhapsHole->start_row + 1); ++k) {
            if ((k + 1) < (perhapsHole->end_row - perhapsHole->start_row + 1)) {
                //绝对值大于阀值，洞有缺口
                if (ceil(left_edge_distance[k] - left_edge_distance[k + 1]) > hole_edge_distance ||
                    ceil(right_edge_distance[k] - right_edge_distance[k + 1]) > hole_edge_distance) {
                    perhapsHole->is_real_hole = false;
                }
            }
        }



        //判断洞顶跟洞底是否闭合
        //算出洞顶下一行的长度
        int hole_top_next_length = right_edge_distance[0] - left_edge_distance[0] + 1;
        //跳到洞顶行首像素
        get_pix_pointer(perhapsHole->start_row - 1, 1, first_pix, I, &temp_pix);
        for (int i1 = 0; i1 < I->cols; ++i1) {
            //找到左边边缘元素
            if (true == temp_pix->is_word_pix) {
                //洞顶左侧边缘点的_offet距离又必须小于或等于left_edge_distance[0]
                if (temp_pix->start_offset > (left_edge_distance[0] + hole_edge_distance)) {
                    perhapsHole->is_real_hole = false;

                } else {
                    //封顶字体像素的连续长度，必须大于阀值；
                    //先保留左边第N个最靠近边缘的像素指针；
                    struct picture_pix *left_top_pix;
                    for (int j = 0; j < (left_edge_distance[0] + 1 - temp_pix->start_offset); ++j) {
                        temp_pix = temp_pix->next_pix;
                    }
                    left_top_pix = temp_pix;

                    //循环找到右边边缘像素指针
                    struct picture_pix *right_top_pix;
                    for (int i = 0; i < I->cols; ++i) {
                        temp_pix = temp_pix->next_pix;
                        if (false == temp_pix->is_word_pix) {
                            right_top_pix = temp_pix;
                            break;
                        }
                    }

                    if ((right_top_pix->start_offset - left_top_pix->start_offset + 1 + hole_edge_distance) <
                        hole_top_next_length) {
                        perhapsHole->is_real_hole = false;
                    }
                }
                break;
            }
            temp_pix = temp_pix->next_pix;
        }

        //检测洞底是否闭合
        //接近洞口的长度
        int hole_bottom_next_length = right_edge_distance[perhapsHole->end_row - perhapsHole->start_row] -
                                      left_edge_distance[perhapsHole->end_row - perhapsHole->start_row] + 1;
        //跳到洞顶行首像素
        get_pix_pointer(perhapsHole->end_row + 1, 1, first_pix, I, &temp_pix);
        for (int k1 = 0; k1 < I->cols; ++k1) {

            //找到左边边缘元素
            if (true == temp_pix->is_word_pix) {
                //洞顶右侧边缘点的_offet距离又必须小于或等于 left_edge_distance[perhapsHole->end_row - perhapsHole->start_row]
                if (temp_pix->start_offset >
                    (left_edge_distance[perhapsHole->end_row - perhapsHole->start_row] + hole_edge_distance)) {
                    perhapsHole->is_real_hole = false;

                } else {
                    //封底字体像素的连续长度，必须大于阀值；
                    //先保留左边第N个最靠近边缘的像素指针；
                    struct picture_pix *left_bottom_pix;
                    for (int j = 0; j < (left_edge_distance[0] + 1 - temp_pix->start_offset); ++j) {
                        temp_pix = temp_pix->next_pix;
                    }
                    left_bottom_pix = temp_pix;

                    //循环找到右边边缘像素指针
                    struct picture_pix *right_top_pix;
                    for (int i = 0; i < I->cols; ++i) {
                        temp_pix = temp_pix->next_pix;
                        if (false == temp_pix->is_word_pix) {
                            right_top_pix = temp_pix;
                            break;
                        }
                    }

                    if ((right_top_pix->start_offset - left_bottom_pix->start_offset + 1 + hole_edge_distance) <
                        hole_bottom_next_length) {
                        perhapsHole->is_real_hole = false;
                    }
                }
                break;
            }
            temp_pix = temp_pix->next_pix;
        }


        if ((perhaps_hole_number - 1) != l) {
            perhapsHole = perhapsHole->next_perhasps_hole;

        }

    }
    //真实洞的数量
    int hole_num = 0;
    perhapsHole = firstPerhapsHole;
    for (int l1 = 0; l1 < perhaps_hole_number; ++l1) {
        if (true == perhapsHole->is_real_hole) {
            hole_num++;
        }
        if ((perhaps_hole_number - 1) != l1) {
            perhapsHole = perhapsHole->next_perhasps_hole;
        }
        //todo,检测这里为什么free不了
        //free(perhapsHole);
    }
    return hole_num;

}

//从树状矩阵获取某个像素指针的函数
void get_pix_pointer(int row, int col, struct picture_pix *first_pix, Mat *I, struct picture_pix **find_pix) {
    struct picture_pix *temp_pix = first_pix;

    for (int i = 1; i <= I->rows * I->cols; ++i) {
        //找到行
        if (temp_pix->location_row != row) {
            temp_pix = temp_pix->next_pix;
        } else {
            //再找列
            if (temp_pix->start_offset != col - 1) {
                temp_pix = temp_pix->next_pix;
            } else {
                //跳出循环
                break;
            }
        }
    }
    *find_pix = temp_pix;
}

//判断这个左距离是不是从大到小，是不是呈现对称分布。
void check_top_big_bottom(bool *top_big_bottom, int I_rows, int I_cols, struct picture_pix *first_pix,
                          pthread_cond_t *check_top_big_bottom_cond, int *tourch_min_center_calculate_num_count) {
    struct picture_pix *temp_pix = first_pix;

    /*
    //获取左侧第一个字体像素相对于行首像素的偏移距离
    int left_distance_array[I_rows];

    int currnt_left_distance_index = 0;
    for (int i = 1; i <= I_rows * I_cols; ++i) {
        //找到一个左侧字体像素，立即跳到下一行
        if (true == temp_pix->is_word_pix) {

            left_distance_array[currnt_left_distance_index] = temp_pix->start_offset;
            currnt_left_distance_index++;
            int current_row = temp_pix->location_row;
            for (int j = 0; j < I_cols; ++j) {
                temp_pix = temp_pix->next_pix;

                //已经跳到下一行
                if (current_row != temp_pix->location_row) {
                    //返回
                    break;
                } else {
                    //注意，这里不是下一行 才加。
                    i++;
                }
            }
        } else {
            temp_pix = temp_pix->next_pix;
        }
    }
    struct thread_top_big_bottom_check_strcut *left_index_data = (struct thread_top_big_bottom_check_strcut *) malloc(
            sizeof(struct thread_top_big_bottom_check_strcut));
    left_index_data->start_index = 0;
    left_index_data->end_index = I_rows - 1;
    left_index_data->distance_array = left_distance_array;
    left_index_data->top_big_bottom = top_big_bottom;
    left_index_data->check_top_big_bottom_cond = check_top_big_bottom_cond;
    left_index_data->tourch_min_center_calculate_num_count = tourch_min_center_calculate_num_count;

    //start_index 跟end_index 产生变化
    pthread_t thread_id_check_left_top_big_bottom;
    int rest = pthread_create(&thread_id_check_left_top_big_bottom, NULL, thread_check_top_big_bottom, left_index_data);
    if (0 != rest) {
        handle_error_en(rest, "创建线程失败 ----\n");
    }
     */

    //获取右侧第一个字体像素相对于行首像素的偏移距离
    //temp_pix 重新指向头指针。
    temp_pix = first_pix;
    int *right_distance_array[I_rows];
    //从最后一个元素往回计算
    int currnt_right_distance_index = 0;
    for (int i = 1; i <= I_rows * I_cols; ++i) {
        //找到一个右侧字体像素，立即跳到上一行
        if (true == temp_pix->is_word_pix) {
            right_distance_array[currnt_right_distance_index] = (int *) malloc(sizeof(int));

            *(right_distance_array[currnt_right_distance_index]) = I_cols - temp_pix->start_offset;
            currnt_right_distance_index++;
            int current_row = temp_pix->location_row;
            for (int j = 0; j < I_cols; ++j) {
                temp_pix = temp_pix->prev_pix;

                //已经跳到下一行
                if (current_row != temp_pix->location_row) {
                    //返回
                    break;
                } else {
                    //注意，这里不是下一行 才加。
                    i++;
                }
            }
        } else {
            temp_pix = temp_pix->prev_pix;
        }
    }

    //倒序一下这个数组
    for (int k = 0; k < (I_rows / 2); k++) {
        int temp = *(right_distance_array[k]);
        *(right_distance_array[k]) = *(right_distance_array[I_rows - 1 - k]);
        *(right_distance_array[I_rows - 1 - k]) = temp;
    }

    struct thread_top_big_bottom_check_strcut *right_index_data = (struct thread_top_big_bottom_check_strcut *) malloc(
            sizeof(struct thread_top_big_bottom_check_strcut));
    right_index_data->start_index = 0;
    right_index_data->end_index = I_rows - 1;
    right_index_data->distance_array = right_distance_array;
    right_index_data->top_big_bottom = top_big_bottom;
    right_index_data->check_top_big_bottom_cond = check_top_big_bottom_cond;
    right_index_data->tourch_min_center_calculate_num_count = tourch_min_center_calculate_num_count;

    //start_index 跟end_index 产生变化
    pthread_t thread_id_check_right_top_big_bottom;
    int right_rest = pthread_create(&thread_id_check_right_top_big_bottom, NULL, thread_check_top_big_bottom,
                                    right_index_data);
    if (0 != right_rest) {
        handle_error_en(right_rest, "创建线程失败 ----\n");
    }

    //pthread_join(thread_id_check_left_top_big_bottom, NULL);
    //pthread_join(thread_id_check_right_top_big_bottom, NULL);

}

void *thread_check_top_big_bottom(void *data) {

    struct thread_top_big_bottom_check_strcut *index_data = (struct thread_top_big_bottom_check_strcut *) data;
    float center_value;
    int next_start_index_1;
    int next_end_index_1;

    //判断像素段是奇数还是偶数
    if (((index_data->end_index - index_data->start_index + 1) % 2) == 0) {
        int center_value_index_1 = ((index_data->end_index + 1) / 2) - 1;
        int center_value_index_2 = center_value_index_1 + 1;

        center_value =
                (*(index_data->distance_array[center_value_index_1]) +
                 *(index_data->distance_array[center_value_index_2])) / 2;
        next_start_index_1 = index_data->start_index;
        next_end_index_1 = (index_data->end_index + 1) / 2 - 1;
    } else {
        int center_value_index = (index_data->end_index) / 2;
        center_value = *(index_data->distance_array[center_value_index]);
        next_start_index_1 = index_data->start_index;
        next_end_index_1 = (index_data->end_index) / 2;

    }

    //start跟end之间超过5个像素再进入递归，5是阀值
    if (min_center_calculate_num <= (next_end_index_1 - next_start_index_1 - 1)) {
        int thread_rest;
        //初始化结构体， 进入递归
        struct thread_top_big_bottom_check_strcut *next_index_data_1 = (struct thread_top_big_bottom_check_strcut *) malloc(
                sizeof(struct thread_top_big_bottom_check_strcut));
        next_index_data_1->start_index = next_start_index_1;
        next_index_data_1->end_index = next_end_index_1;
        next_index_data_1->distance_array = index_data->distance_array;
        next_index_data_1->top_big_bottom = index_data->top_big_bottom;
        next_index_data_1->check_top_big_bottom_cond = index_data->check_top_big_bottom_cond;
        next_index_data_1->tourch_min_center_calculate_num_count = index_data->tourch_min_center_calculate_num_count;

        //thread_check_top_big_bottom(next_index_data_1);
        pthread_t thread_id;
        thread_rest = pthread_create(&thread_id, NULL, thread_check_top_big_bottom, next_index_data_1);
        if (0 != thread_rest) {
            handle_error_en(thread_rest, "pthread_create 3");
        }
        int next_start_index_2;
        int next_end_index_2;

        //判断像素段是奇数还是偶数
        if (((index_data->end_index - index_data->start_index + 1) % 2) == 0) {
            next_start_index_2 = (index_data->end_index + 1) / 2;
            next_end_index_2 = index_data->end_index;
        } else {
            next_start_index_2 = (index_data->end_index) / 2;
            next_end_index_2 = index_data->end_index;
        }


        //初始化结构体， 进入递归
        struct thread_top_big_bottom_check_strcut *next_index_data_2 = (struct thread_top_big_bottom_check_strcut *) malloc(
                sizeof(struct thread_top_big_bottom_check_strcut));
        next_index_data_2->start_index = next_start_index_2;
        next_index_data_2->end_index = next_end_index_2;
        next_index_data_2->distance_array = index_data->distance_array;
        next_index_data_2->top_big_bottom = index_data->top_big_bottom;
        next_index_data_2->check_top_big_bottom_cond = index_data->check_top_big_bottom_cond;
        next_index_data_2->tourch_min_center_calculate_num_count = index_data->tourch_min_center_calculate_num_count;

        //thread_check_top_big_bottom(next_index_data_2);
        pthread_t thread_id_2;
        int thread_rest_2 = pthread_create(&thread_id_2, NULL, thread_check_top_big_bottom, next_index_data_2);

        if (0 != thread_rest_2) {
            handle_error_en(thread_rest_2, "pthread_create 4");
        }

    } else {
        //触碰阀值
        //*(index_data->tourch_min_center_calculate_num_count) += 1;
        __sync_fetch_and_add((*index_data).tourch_min_center_calculate_num_count, 1);
        if (2 == *(index_data->tourch_min_center_calculate_num_count)) {
            pthread_cond_signal(index_data->check_top_big_bottom_cond);
        }
    }

    if (*(index_data->distance_array[index_data->end_index]) > center_value ||
        *(index_data->distance_array[index_data->start_index]) < center_value) {
        *(index_data->top_big_bottom) = false;
    }
    //todo,distance_array是malloc出来的，要free掉

    free(index_data);

}

int transform_to_mstrcut(Mat &I, struct picture_pix **first_pix) {
    // accept only char type matrices
    CV_Assert(I.depth() == CV_8U);

    int channels = I.channels();

    int nRows = I.rows;
    int nCols = I.cols * channels;

    if (I.isContinuous()) {
        nCols *= nRows;
        nRows = 1;
        int i, j;
        uchar *p;
        int current_pix = 1; //现在遍历到那个像素了
        int pix_number = I.rows * I.cols; //全部的像素数量
        for (i = 0; i < nRows; ++i) {
            p = I.ptr<uchar>(i);
            /*
            //选取第一点作为背景色
            struct picture_pix background_pix;
            background_pix.B = p[0];
            background_pix.G = p[1];
            background_pix.R = p[2];
             */
            //第一点不太好，以后再想办法算出背景色，先默认白色
            struct picture_pix background_pix;
            background_pix.B = 255;
            background_pix.G = 255;
            background_pix.R = 255;

            struct picture_pix *pix_array[pix_number];

            for (int current_pix_index = 1; current_pix_index <= pix_number; ++current_pix_index) {

                pix_array[current_pix_index - 1] = (struct picture_pix *) malloc(sizeof(struct picture_pix));

                int B_index = current_pix_index * 3 - 3;
                int G_index = current_pix_index * 3 - 2;
                int R_index = current_pix_index * 3 - 1;


                pix_array[current_pix_index - 1]->B = p[B_index];
                pix_array[current_pix_index - 1]->G = p[G_index];
                pix_array[current_pix_index - 1]->R = p[R_index];

                float temp_location_row = (float) current_pix_index / (float) I.cols;
                pix_array[current_pix_index - 1]->location_row = (int) ceil(temp_location_row);

                pix_array[current_pix_index - 1]->start_offset =
                        current_pix_index - 1 - ((pix_array[current_pix_index - 1]->location_row - 1) * I.cols);
                check_is_word_pix(&background_pix, pix_array[current_pix_index - 1]);

            }
            //再循环一次做指针指向
            for (int current_pix_index = 0; current_pix_index < pix_number; ++current_pix_index) {
                //头尾指向
                if (0 == current_pix_index) {
                    pix_array[current_pix_index]->prev_pix = pix_array[pix_number - 1];
                } else {
                    pix_array[current_pix_index]->prev_pix = pix_array[current_pix_index - 1];
                }

                if ((pix_number - 1) == current_pix_index) {
                    pix_array[current_pix_index]->next_pix = pix_array[0];
                } else {
                    pix_array[current_pix_index]->next_pix = pix_array[current_pix_index + 1];
                }

            }
            *first_pix = pix_array[0];


        }
        //todo，非连续的图片暂时不处理
    } else {

    }

    return 0;
};

int check_is_word_pix(struct picture_pix *backgound_pix, struct picture_pix *current_pix) {
    //单色最大差距
    int single_max_gap = 80;
    //双色最大差距
    int double_max_gap = 50;
    //三色最大差距
    int three_max_gap = 30;

    int B_gap = abs(backgound_pix->B - current_pix->B);
    int G_gap = abs(backgound_pix->G - current_pix->G);
    int R_gap = abs(backgound_pix->R - current_pix->R);
    //先检测单色的差距
    if (B_gap > single_max_gap || G_gap > single_max_gap || R_gap > single_max_gap) {
        current_pix->is_word_pix = true;

    } else if ((B_gap > double_max_gap && G_gap > double_max_gap) ||
               (B_gap > double_max_gap && R_gap > double_max_gap) ||
               (G_gap > double_max_gap && R_gap > double_max_gap)) {
        current_pix->is_word_pix = true;
    } else if (B_gap > three_max_gap || G_gap > three_max_gap || R_gap > three_max_gap) {
        current_pix->is_word_pix = true;
    } else {
        current_pix->is_word_pix = false;
    };

    return 0;
};


