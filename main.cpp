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
int transform_to_mstrcut(Mat &I, struct picture_pix *pic_pix);

//todo，判断当前像素是否是字体像素，以后可能要根据前后的连续像素做判断，先封装成函数
int check_is_word_pix(struct picture_pix *backgound_pix, struct picture_pix *current_pix);

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

    struct picture_pix *first_pix = (struct picture_pix *) malloc(sizeof(struct picture_pix));
    transform_to_mstrcut(I, first_pix);

    cv::Mat clone_i = I.clone();
    J = ScanImageAndReduceC(clone_i);
    imwrite("/home/loken/ClionProjects/opencv_demo/cpp-test/A_2.jpg", J);


}

int transform_to_mstrcut(Mat &I, struct picture_pix *first_pix) {
    // accept only char type matrices
    CV_Assert(I.depth() == CV_8U);

    int channels = I.channels();

    int nRows = I.rows;
    int nCols = I.cols * channels;

    if (I.isContinuous()) {
        //cout << "I.isContinuous() is true " << endl;
        nCols *= nRows;
        nRows = 1;
        int i, j;
        uchar *p;
        //todo,int 可能存不下所有像素，以后想办法
        int current_pix = 1; //现在遍历到那个像素了
        int pix_number = I.rows * I.cols; //全部的像素数量
        for (i = 0; i < nRows; ++i) {
            p = I.ptr<uchar>(i);
            //选取第一点作为背景色
            struct picture_pix background_pix;
            background_pix.B = p[0];
            background_pix.G = p[1];
            background_pix.R = p[2];

            struct picture_pix * pix_array[pix_number];

            for (int current_pix_index = 1; current_pix_index <= pix_number; ++current_pix_index) {
                printf("current_pix_index is %d\n",current_pix_index);

                pix_array[current_pix_index-1] = (struct picture_pix *) malloc(sizeof(struct picture_pix));

                int B_index = current_pix_index * 3 - 3;
                int G_index = current_pix_index * 3 - 2;
                int R_index = current_pix_index * 3 - 1;

                printf("p[B_index] is %d ,p[G_index] is %d ,p[R_index] is %d \n",p[B_index],p[G_index],p[R_index]);

                pix_array[current_pix_index-1]->B = p[B_index];
                pix_array[current_pix_index-1]->G = p[G_index];
                pix_array[current_pix_index-1]->R = p[R_index];
                pix_array[current_pix_index-1]->location_row = ceil(current_pix_index / I.cols);
                pix_array[current_pix_index-1]->start_offset = current_pix_index - (pix_array[current_pix_index-1]->location_row * I.cols);
                check_is_word_pix(&background_pix, pix_array[current_pix_index-1]);

                /*
                if (1 == current_pix_index) {
                    //切换头指针指向
                    struct picture_pix *first_pix_back_free = first_pix;
                    first_pix = current_pix;
                    //free(first_pix_back_free);
                }

                //获取前一个像素的指向
                if (1 != current_pix_index) {
                    struct picture_pix *current_prev_pix = first_pix;
                    //通过第一个元素获取前一个元素的指针
                    for (int i = 1; i < (current_pix_index - 1); i++) {
                        current_prev_pix = current_prev_pix->next_pix;
                    }
                    current_pix->prev_pix = current_prev_pix;
                }

                //遍历到最后一个元素，连接上首像素
                if (current_pix_index == pix_number) {
                    first_pix->prev_pix = current_pix;
                }
                else {
                    int current_next_pix_index = current_pix_index + 1;

                    struct picture_pix *next_pix = (struct picture_pix *) calloc(1, sizeof(struct picture_pix));

                    int next_B_index = current_next_pix_index * 3 - 3;
                    int next_G_index = current_next_pix_index * 3 - 2;
                    int next_R_index = current_next_pix_index * 3 - 1;
                    next_pix->B = p[next_B_index];
                    next_pix->G = p[next_G_index];
                    next_pix->R = p[next_R_index];
                    next_pix->location_row = ceil(current_next_pix_index / I.cols);
                    next_pix->start_offset = current_next_pix_index - (next_pix->location_row * I.cols);
                    check_is_word_pix(&background_pix, next_pix);

                    current_pix->next_pix = next_pix;
                }
                 */

            }
            //再循环一次做指针指向
            for (int current_pix_index = 0; current_pix_index < pix_number; ++current_pix_index) {
                //头尾指向
                if( 0 == current_pix_index  ){
                    pix_array[current_pix_index]->prev_pix = pix_array[pix_number-1];
                }else{
                    pix_array[current_pix_index]->prev_pix = pix_array[current_pix_index-1];
                }

                if( (pix_number-1) == current_pix_index  ){
                    pix_array[current_pix_index]->next_pix = pix_array[0];
                }else{
                    pix_array[current_pix_index]->next_pix = pix_array[current_pix_index+1];
                }

            }

            first_pix = pix_array[0];
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
        current_pix->is_word_pix = false;

    } else if ((B_gap > double_max_gap && G_gap > double_max_gap) ||
               (B_gap > double_max_gap && R_gap > double_max_gap) ||
               (G_gap > double_max_gap && R_gap > double_max_gap)) {
        current_pix->is_word_pix = false;
    } else if (B_gap > three_max_gap || G_gap > three_max_gap || R_gap > three_max_gap) {
        current_pix->is_word_pix = false;
    } else {
        current_pix->is_word_pix = true;
    };

    return 0;
};


