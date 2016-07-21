#include <stdio.h>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

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

int thread_check_top_big_bottom(int test) {


    /*
    //判断是奇数还是偶数
    if (end_value_index % 2 == 0) {
        center_value =
                (left_distance_array[(end_value_index / 2) - 1] + left_distance_array[(end_value_index / 2)]) / 2;
    } else {
        center_value = left_distance_array[(end_value_index - 1) / 2];
    }
    if (left_distance_array[end_value_index] > center_value ||
        left_distance_array[start_value_index] < center_value) {
        top_big_bottom = false;
    }
     */

    //start_index 跟end_index 产生变化

    // pthread_create(&thread_id_1, NULL, (void *) thread_check_top_big_bottom, &index_data);
    return 0;
}


int main(int argc, char **argv) {

    if (argc < 2) {
        cout << "Not enough parameters" << endl;
        return -1;
    }

    Mat I, J;

    I = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    cout << "E = " << endl << " " << I << endl << endl;


    if (!I.data) {
        cout << "The image" << argv[1] << " could not be loaded." << endl;
        return -1;
    }

    struct picture_pix *first_pix;
    transform_to_mstrcut(I, &first_pix);

    //todo,二值计算要封装成函数

    //获取左侧第一个字体像素相对于行首像素的偏移距离
    int left_distance_array[I.rows];
    struct picture_pix *temp_pix = first_pix;
    int currnt_left_distance_index = 0;
    for (int i = 1; i <= I.cols * I.rows; ++i) {
        //找到一个左侧字体像素，立即跳到下一行
        if (true == temp_pix->is_word_pix) {

            left_distance_array[currnt_left_distance_index] = temp_pix->start_offset;
            currnt_left_distance_index++;
            int current_row = temp_pix->location_row;
            for (int j = 0; j < I.cols; ++j) {
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


    bool top_big_bottom = true;
    //中间值
    float center_value;
    //最后元素的下标
    int end_value_index = I.rows;
    int start_value_index = 0;

    //二值化计算像素数量最少值，超过这阀值做中间计算才有价值。
    int min_center_calculate_num = 5;

    pthread_t thread_id_1;

    struct thread_top_big_bottom_check_strcut {
        int start_index;
        int end_index;
    };


    //循环判断这个左距离是不是从大到小


    struct thread_top_big_bottom_check_strcut index_data;
    index_data.start_index = 0;
    index_data.end_index = I.rows;



    //start_index 跟end_index 产生变化
    int test;
    int test2 = 45;
    test = pthread_create(&thread_id_1, NULL, thread_check_top_big_bottom, 15);


    printf("sdsdsd\n");
    return 0;

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
            //选取第一点作为背景色
            struct picture_pix background_pix;
            background_pix.B = p[0];
            background_pix.G = p[1];
            background_pix.R = p[2];

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


