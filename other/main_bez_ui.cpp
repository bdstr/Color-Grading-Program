#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <math.h>

using namespace cv;
using namespace std;

// Indexy kolorów w zmiennej typu Vec3b
#define RED 2
#define GREEN 1
#define BLUE 0

// Luminacja poszczególnych kolorów
#define RED_LUMINANCE 0.299
#define GREEN_LUMINANCE 0.587
#define BLUE_LUMINANCE 0.114

struct Settings {
    float gamma = 1.0;
    int contrast = 0;
    int brightness = 0;
    float exposure = 0.0;
    float saturation = 1.0;
    int colorTemperature = 0;
    int hue[3] = {0, 0, 0};
    string outputPath;
};

int valueInRange(int pixelValue){
    if(pixelValue <= 255 && pixelValue >= 0)
    {
        return pixelValue;
    }
    else if(pixelValue < 0)
    {
        return 0;
    }
    else
    {
        return 255;
    }
}

// -255 - 255, neutral = 0
int brightness(int inputValue, int brightnessOffset)
{
    return valueInRange(inputValue + brightnessOffset); 
}

// -255 - 255, neutral = 0
int contrast(int inputValue, int contrastValue)
{
    float factor = (259.0 * (contrastValue + 255.0)) / (255.0 * (259.0 - contrastValue));
    return valueInRange(factor * (inputValue - 128) + 128 );
}

// 0.0 - 4.0, neutral = 1.0
int gamma(int inputValue, float gammaValue)
{
    return valueInRange(pow(inputValue / 255.0, gammaValue) * 255.0);
}

// -4.0 - 4.0, neutral = 0.0
int exposure(int inputValue, float exposureValue)
{
    return valueInRange(inputValue * pow(2, exposureValue));
}


// -255 - 255, neutral = 0
Vec3b hue(Vec3b colorVector, int redOffset, int greenOffset, int blueOffset)
{
    colorVector[RED] = brightness(colorVector[RED], redOffset);
    colorVector[GREEN] = brightness(colorVector[GREEN], greenOffset);
    colorVector[BLUE] = brightness(colorVector[BLUE], blueOffset);

    return colorVector;
}

// -255 - 255, neutral = 0
Vec3b colorTemperature(Vec3b colorVector, int temperatureOffset)
{
    return hue(colorVector, temperatureOffset, 0, -(temperatureOffset));
}

// 0.0 - 4.0, neutral = 1.0
Vec3b saturation(Vec3b colorVector, float saturationValue)
{
    float grey = (colorVector[RED] * RED_LUMINANCE) + (colorVector[GREEN] * GREEN_LUMINANCE) + (colorVector[BLUE] * BLUE_LUMINANCE);

    colorVector[RED] = valueInRange(grey + saturationValue * (colorVector[RED] - grey));
    colorVector[GREEN] = valueInRange(grey + saturationValue * (colorVector[GREEN] - grey));
    colorVector[BLUE] = valueInRange(grey + saturationValue * (colorVector[BLUE] - grey));
    
    return colorVector;
}

void createLookUpTable(Settings *userSettings, const Settings *defaultSettings, int *lookUpTable)
{
    for(int colorIndex = 0; colorIndex < 256; colorIndex++)
    {
        Vec3b colorVector;

        for(int colorChannel = 0; colorChannel <= 2; colorChannel++)
        {
            colorVector[colorChannel] = colorIndex;
            if(userSettings->gamma != defaultSettings->gamma)
                colorVector[colorChannel] = gamma(colorVector[colorChannel], userSettings->gamma);
            if(userSettings->contrast != defaultSettings->contrast)
                colorVector[colorChannel] = contrast(colorVector[colorChannel], userSettings->contrast);
            if(userSettings->brightness != defaultSettings->brightness)
                colorVector[colorChannel] = brightness(colorVector[colorChannel], userSettings->brightness);
            if(userSettings->exposure != defaultSettings->exposure)
                colorVector[colorChannel] = exposure(colorVector[colorChannel], userSettings->exposure);
        }

        if(userSettings->colorTemperature != defaultSettings->colorTemperature)
            colorVector = colorTemperature(colorVector, userSettings->colorTemperature);
        if(userSettings->hue[RED] != defaultSettings->hue[RED] || userSettings->hue[GREEN] != defaultSettings->hue[GREEN] || userSettings->hue[BLUE] != defaultSettings->hue[BLUE])
            colorVector = hue(colorVector, userSettings->hue[RED], userSettings->hue[GREEN], userSettings->hue[BLUE]);

        *((lookUpTable + colorIndex * 3) + RED) = colorVector[RED];
        *((lookUpTable + colorIndex * 3) + GREEN) = colorVector[GREEN];
        *((lookUpTable + colorIndex * 3) + BLUE) = colorVector[BLUE];
    }
}

void transformImage(Mat image, Settings *userSettings, const Settings *defaultSettings, int *lookUpTable)
{
    for(int y = 0; y < image.rows; y++)
    {
        for(int x = 0; x < image.cols; x++)
        {
            Vec3b color = image.at<Vec3b>(Point(x,y));

            if(userSettings->saturation != defaultSettings->saturation)
                color = saturation(color, userSettings->saturation);

            for(int i = 0; i <= 2; i++)
            {
                color[i] = *((lookUpTable + color[i] * 3) + i);
            }

            image.at<Vec3b>(Point(x,y)) = color;
        }
    }
}

bool checkArgumentInt(char **argv, int *argc, int i, string flag, int *userSetting, int valueMin, int valueMax)
{
    if((string)argv[i] == flag && (i + 1) < *argc){
        int argVal = stoi(argv[i + 1]);
        if( argVal < valueMax && argVal > valueMin)
        {
            *userSetting = argVal;
        }
        else
        {
            cout << "Błędna wartość!" << endl;
            return true;
        }
    }
    return false;
}

bool checkArgumentFloat(char **argv, int *argc, int i, string flag, float *userSetting, float valueMin, float valueMax)
{
    if((string)argv[i] == flag && (i + 1) < *argc){
        float argVal = stof(argv[i + 1]);
        if( argVal <= valueMax && argVal >= valueMin)
        {
            *userSetting = argVal;
        }
        else
        {
            cout << "Błędna wartość!" << endl;
            return true;
        }
    }
    return false;
}

bool checkArgumentString(char **argv, int *argc, int i, string flag, string *userSetting)
{
    if((string)argv[i] == flag && (i + 1) < *argc){
        string argVal = (string)argv[i + 1];
        if(argVal.size() > 0)
        {
            *userSetting = argVal;
        }
        else
        {
            cout << "Błędna wartość!" << endl;
            return true;
        }
    }
    return false;
}


int main( int argc, char** argv )
{

// Benchmarking
    clock_t start;
    double duration;
    start = clock();

// Inicjalizacja zmiennych
    Mat image;
    Mat imageOriginal;
    Settings userSettings;
    const Settings defaultSettings;
    int lookUpTable[256][3];
    String imageName;

    GtkWidget *okno;
    gtk_init (&argc, &argv);
    okno = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_show (okno);
    gtk_main ();

// Odczyt flag z linii poleceń
    if( argc > 1)
    {
        imageName = argv[1];
        for(int i = 2; i < argc; i++)
        {
            if( checkArgumentInt(&argv[0], &argc, i, "-b", &userSettings.brightness, -256, 256) ) return -1;
            if( checkArgumentInt(&argv[0], &argc, i, "-c", &userSettings.contrast, -256, 256) ) return -1;
            if( checkArgumentInt(&argv[0], &argc, i, "-t", &userSettings.colorTemperature, -256, 256) ) return -1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hr", &userSettings.hue[RED], -256, 256) ) return -1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hg", &userSettings.hue[GREEN], -256, 256) ) return -1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hb", &userSettings.hue[BLUE], -256, 256) ) return -1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-e", &userSettings.exposure, -4.0, 4.0) ) return -1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-s", &userSettings.saturation, 0.0, 4.0) ) return -1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-g", &userSettings.gamma, 0.0, 4.0) ) return -1;
            if( checkArgumentString(&argv[0], &argc, i, "-o", &userSettings.outputPath) ) return -1;
        }
    }
// Uruchamianie interfejsu graficznego
    else
    {
        cout << "Interfejs graficzny w budowie!" << endl;
        return -1;
    }
    
// Otwieranie zdjęcia
    image = imread(samples::findFile(imageName, false, true), IMREAD_COLOR);
    if(image.empty())
    {
        cout <<  "Nie można otworzyć lub znaleźć pliku o nazwie " << imageName << "!" << endl ;
        return -1;
    }
    imageOriginal = image.clone();


    /*
    userSettings.gamma = 1.0;
    userSettings.contrast = 0;
    userSettings.brightness = 0;
    userSettings.exposure = 0.0;
    userSettings.saturation = 1.0;
    userSettings.colorTemperature = 0;
    userSettings.hue[RED] = 0;
    userSettings.hue[GREEN] = 0;
    userSettings.hue[BLUE] = 0;
    userSettings.outputPath;
    */


// Transformowanie zdjęcia
    createLookUpTable(&userSettings, &defaultSettings, &lookUpTable[0][0]);
    transformImage(image, &userSettings, &defaultSettings, &lookUpTable[0][0]);

// Zapis zdjęcia do pliku
    if(userSettings.outputPath.size() > 0)
    {
        cout << "Zapisano do pliku " << userSettings.outputPath << endl;
        imwrite(userSettings.outputPath, image);
    }
// Otworzenie zdjęcia w oknie
    else
    {
        namedWindow("Color Grading Program", WINDOW_NORMAL);
        imshow("Color Grading Program", image);

        namedWindow("ORYGINAŁ", WINDOW_NORMAL);
        imshow("ORYGINAŁ", imageOriginal);

        resizeWindow("Color Grading Program", 960, 1080);
        moveWindow("Color Grading Program", 960, 0);
        resizeWindow("ORYGINAŁ", 960, 1080);
        moveWindow("ORYGINAŁ", 0, 0);

        waitKey(0); // Wait for a keystroke in the window
    }

// Benchmarking
    duration = ( clock() - start ) / (double) CLOCKS_PER_SEC;
    cout << "Render zdjęcia: "<< duration << "s" << endl;

    return 0;
}