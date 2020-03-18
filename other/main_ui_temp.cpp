/*

Polecenie kompilacji:
g++ main.cpp -Wall -Wextra `pkg-config opencv4 gtk+-2.0 --cflags --libs` -o "Color Grading Program"

*/

#include <iostream>
#include <stdio.h>
#include <math.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <gtk/gtk.h>

using namespace cv;
using namespace std;


// -------------------
//  DEFINICJE STAŁYCH
// -------------------

// Indexy kolorów w zmiennej typu Vec3b
#define RED 2
#define GREEN 1
#define BLUE 0

// Luminacja poszczególnych kolorów
#define RED_LUMINANCE 0.299
#define GREEN_LUMINANCE 0.587
#define BLUE_LUMINANCE 0.114

// Tymczasowe pliki ze zdjęciami
#define TEMP_FILE "resources/temp.jpg"
#define TEMP_FILE_ORIGINAL "resources/temp_original.jpg"

// Plik ze schematem interfejsu
#define UI_FILE "resources/ui.glade"

// Struktura przechowująca ustawienia
struct Settings
{
    float gamma = 1.0;
    int contrast = 0;
    int brightness = 0;
    float exposure = 0.0;
    float saturation = 1.0;
    int colorTemperature = 0;
    int hue[3] = {0, 0, 0};
    string outputPath;
};

// Struktura przechowująca wskaźniki na ustawienia oraz obiekt przechowujący elementy interfejsu
struct AppData
{
    Mat image;
    Mat imageOriginal;
    Settings *userSettings;
    const Settings *defaultSettings;
    int *lookUpTable;
    String *imageName;
    string tempFilenamePath = TEMP_FILE;
    string tempFilenameOriginalPath = TEMP_FILE_ORIGINAL;
    bool displayOriginalPhoto = false;
    GtkBuilder **builder;
    GtkFileChooserButton **chooseFileButton;
    GObject **brightnessButton;
    GObject **contrastButton;
    GObject **exposureButton;
    GObject **saturationButton;
    GObject **gammaButton;
    GObject **temperatureButton;
    GObject **hueRedButton, **hueGreenButton, **hueBlueButton;
};


// -----------------------------------------
//  PODSTAWOWE FUNKCJE OPERUJĄCE NA ZDJĘCIU
// -----------------------------------------

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

int brightness(int inputValue, int brightnessOffset)
{
    return valueInRange(inputValue + brightnessOffset); 
}

int contrast(int inputValue, int contrastValue)
{
    float factor = (259.0 * (contrastValue + 255.0)) / (255.0 * (259.0 - contrastValue));
    return valueInRange(factor * (inputValue - 128) + 128 );
}

int gamma(int inputValue, float gammaValue)
{
    return valueInRange(pow(inputValue / 255.0, gammaValue) * 255.0);
}

int exposure(int inputValue, float exposureValue)
{
    return valueInRange(inputValue * pow(2, exposureValue));
}


Vec3b hue(Vec3b colorVector, int redOffset, int greenOffset, int blueOffset)
{
    colorVector[RED] = brightness(colorVector[RED], redOffset);
    colorVector[GREEN] = brightness(colorVector[GREEN], greenOffset);
    colorVector[BLUE] = brightness(colorVector[BLUE], blueOffset);

    return colorVector;
}

Vec3b colorTemperature(Vec3b colorVector, int temperatureOffset)
{
    return hue(colorVector, temperatureOffset, 0, -(temperatureOffset));
}

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


// -----------------------------------------------
//  FUNKCJE POZWALAJĄCE NA TRASFORMOWANIE ZDJĘCIA
// -----------------------------------------------

bool openFile(Mat &image, Mat &imageOriginal, string *imageName)
{
    image = imread(samples::findFile(*imageName, false, true), IMREAD_COLOR);
    if(image.empty())
    {
        cout <<  "Nie można otworzyć lub znaleźć pliku o nazwie " << *imageName << "!" << endl ;
        return false;
    }
    imageOriginal = image.clone();
    return true;
}

bool saveFile(Mat image, string *outputPath)
{
    if(imwrite(*outputPath, image))
    {
        cout <<  "Plik zapisany w ścieżce " << *outputPath << "!" << endl ;
        return true;
    }
    cout <<  "Nie udało się zapisać pliku w ścieżce " << *outputPath << "!" << endl ;
    return false;
}

void updateImageWithSettings(Mat &image, Mat &imageOriginal, Settings *userSettings, const Settings *defaultSettings, int *lookUpTable)
{
// Benchmarking
    clock_t start;
    double duration;
    start = clock();

// Transformowanie zdjęcia
    createLookUpTable(userSettings, defaultSettings, lookUpTable);
    image = imageOriginal.clone();
    transformImage(image, userSettings, defaultSettings, lookUpTable);

// Benchmarking
    duration = (clock() - start) / (double)CLOCKS_PER_SEC;
    cout << "Render zdjęcia: "<< duration << "s" << endl;
}


// ---------------------------------------------------
//  FUNKCJE SPRAWDZAJĄCE ARGUMENTY WCZYTANE Z KONSOLI
// ---------------------------------------------------

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


// ------------------------------------------
//  FUNKCJE OBSŁUGUJĄCE INTERFEJST GRAFICZNY
// ------------------------------------------

void saveButtonValueInt(GtkWidget *widget, gpointer data)
{
    double buttonValue = gtk_spin_button_get_value((GtkSpinButton *)widget);
    *(int *)data = (int)buttonValue;
}

void saveButtonValueFloat(GtkWidget *widget, gpointer data)
{
    double buttonValue = gtk_spin_button_get_value((GtkSpinButton *)widget);
    *(float *)data = (float)buttonValue;
}

void showOnButtonInt(GObject *button, gpointer value)
{
    int *valueInt = (int *)value;
    double valueDouble = (double)*valueInt;
    gtk_spin_button_set_value((GtkSpinButton *)button, valueDouble);
}

void showOnButtonFloat(GObject *button, gpointer value)
{
    float *valueFloat = (float *)value;
    double valueDouble = (double)*valueFloat;
    gtk_spin_button_set_value((GtkSpinButton *)button, valueDouble);
}

void refreshButtonLabels(AppData *appData)
{ 
    gtk_file_chooser_set_filename((GtkFileChooser *)*appData->chooseFileButton, (*appData->imageName).c_str());
    showOnButtonInt(*appData->brightnessButton, &appData->userSettings->brightness);
    showOnButtonInt(*appData->contrastButton, &appData->userSettings->contrast);
    showOnButtonFloat(*appData->exposureButton, &appData->userSettings->exposure);
    showOnButtonFloat(*appData->saturationButton, &appData->userSettings->saturation);
    showOnButtonFloat(*appData->gammaButton, &appData->userSettings->gamma);
    showOnButtonInt(*appData->temperatureButton, &appData->userSettings->colorTemperature);
    showOnButtonInt(*appData->hueRedButton, &appData->userSettings->hue[RED]);
    showOnButtonInt(*appData->hueGreenButton, &appData->userSettings->hue[GREEN]);
    showOnButtonInt(*appData->hueBlueButton, &appData->userSettings->hue[BLUE]);
}

void displayImage(AppData *appData)
{
    GdkPixbuf *pixbuf;
    GtkBuilder *builder = *appData->builder;
    GtkImage *imageContainer = (GtkImage *)gtk_builder_get_object(builder, "imageContainer");

// Jeśli jest wciśnięty przycisk "Podejrzyj oryginał" załaduj oryginalne zdjęcie
    if(appData->displayOriginalPhoto)
    {        
        pixbuf = gdk_pixbuf_new_from_file_at_size(appData->tempFilenameOriginalPath.c_str(), 890, 710, NULL);
    }
    else
    {
        pixbuf = gdk_pixbuf_new_from_file_at_size(appData->tempFilenamePath.c_str(), 890, 710, NULL);
    }

    gtk_image_set_from_pixbuf(imageContainer, pixbuf);

// Zwalnianie pamięci
    g_object_unref(pixbuf);
}

void loadImage(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;

    string filename = gtk_file_chooser_get_filename((GtkFileChooser *)widget);
    *appData->imageName = filename;
    if( openFile(appData->image, appData->imageOriginal, appData->imageName) )
    {
    // Tworzenie zdjęć tymczasowych
        if( !saveFile(appData->image, &appData->tempFilenamePath))
        {
            cout << "Nie można zapisać pliku tymczasowego!" << endl;
        }
        if( !saveFile(appData->image, &appData->tempFilenameOriginalPath))
        {
            cout << "Nie można zapisać pliku tymczasowego!" << endl;
        }
    }

    displayImage(appData);
}

void applySettings(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;
// Transformowanie zdjęcia
    updateImageWithSettings(appData->image, appData->imageOriginal, appData->userSettings, appData->defaultSettings, appData->lookUpTable);
// Tworzenie zdjęcia tymczasowego
    if( !saveFile(appData->image, &appData->tempFilenamePath))
    {
        cout << "Nie można zapisać pliku tymczasowego!" << endl;
    }

    displayImage(appData);
}

void resetSettings(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;

    *appData->userSettings = (Settings)*appData->defaultSettings;
    refreshButtonLabels(appData);
    applySettings(NULL, appData);
}

void displayOriginalImage(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;

    appData->displayOriginalPhoto = !(appData->displayOriginalPhoto);
    displayImage(appData);
}

void exportFile(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;
    
    GtkWidget *fileChooserDialog;
    fileChooserDialog = gtk_file_chooser_dialog_new("Eksportuj plik", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
    
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(fileChooserDialog), (*appData->imageName).c_str());

    if (gtk_dialog_run (GTK_DIALOG (fileChooserDialog)) == GTK_RESPONSE_ACCEPT)
    {
        appData->userSettings->outputPath = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fileChooserDialog));

        if( !saveFile(appData->image, &appData->userSettings->outputPath))
        {
            cout << "Nie udało się wyeksportować pliku!" << endl;
        }
        
        cout << appData->userSettings->outputPath << endl;
    }
    gtk_widget_destroy(fileChooserDialog);
}

void closeWindow(GtkWidget *widget, gpointer data)
{
    AppData *appData = (AppData *)data;
    remove(appData->tempFilenamePath.c_str());
    remove(appData->tempFilenameOriginalPath.c_str());
    gtk_main_quit();
}


// --------------
//  FUNKCJA MAIN
// --------------

int main( int argc, char** argv )
{

// Inicjalizacja zmiennych
    Mat image;
    Mat imageOriginal;
    Settings userSettings;
    const Settings defaultSettings;
    int lookUpTable[256][3];
    String imageName;

// Odczyt flag z linii poleceń
    if( argc > 1)
    {
        imageName = argv[1];
        for(int i = 2; i < argc; i++)
        {
            if( checkArgumentInt(&argv[0], &argc, i, "-b", &userSettings.brightness, -256, 256) ) return 1;
            if( checkArgumentInt(&argv[0], &argc, i, "-c", &userSettings.contrast, -256, 256) ) return 1;
            if( checkArgumentInt(&argv[0], &argc, i, "-t", &userSettings.colorTemperature, -256, 256) ) return 1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hr", &userSettings.hue[RED], -256, 256) ) return 1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hg", &userSettings.hue[GREEN], -256, 256) ) return 1;
            if( checkArgumentInt(&argv[0], &argc, i, "-hb", &userSettings.hue[BLUE], -256, 256) ) return 1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-e", &userSettings.exposure, -4.0, 4.0) ) return 1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-s", &userSettings.saturation, 0.0, 4.0) ) return 1;
            if( checkArgumentFloat(&argv[0], &argc, i, "-g", &userSettings.gamma, 0.0, 4.0) ) return 1;
            if( checkArgumentString(&argv[0], &argc, i, "-o", &userSettings.outputPath) ) return 1;
        }
    }
    

// Zapis zdjęcia do pliku
    if(userSettings.outputPath.size() > 0)
        {
        if( !openFile(image, imageOriginal, &imageName) ) return 1;
        updateImageWithSettings(image, imageOriginal, &userSettings, &defaultSettings, &lookUpTable[0][0]);
        if( !saveFile(image, &userSettings.outputPath)) return 1;
    }
// Uruchamianie interfejsu graficznego
    else
    {
    // Tworzenie wskaźników na obiekty interfejsu
        GtkBuilder *builder;
        GObject *mainWindow;
        GtkFileChooserButton *chooseFileButton;
        GObject *applyButton;
        GObject *resetButton;
        GObject *changesButton;
        GObject *exportButton;
        GObject *brightnessButton;
        GObject *contrastButton;
        GObject *exposureButton;
        GObject *saturationButton;
        GObject *gammaButton;
        GObject *temperatureButton;
        GObject *hueRedButton, *hueGreenButton, *hueBlueButton;
        GError *error = NULL;

    // Tworzenie struktury ze wszystkimi danymi programu oraz przypisywanie im wartości (także wskaźników na wskaźniki obiektów interfejsu)
        AppData appData;
        appData.userSettings = &userSettings;
        appData.defaultSettings = &defaultSettings;
        appData.image = image;
        appData.imageOriginal = imageOriginal;
        appData.lookUpTable = &lookUpTable[0][0];
        appData.imageName = &imageName;
        appData.builder = &builder;
        appData.chooseFileButton = &chooseFileButton;
        appData.brightnessButton = &brightnessButton;
        appData.contrastButton = &contrastButton;
        appData.exposureButton = &exposureButton;
        appData.saturationButton = &saturationButton;
        appData.gammaButton = &gammaButton;
        appData.temperatureButton = &temperatureButton;
        appData.hueRedButton = &hueRedButton;
        appData.hueGreenButton = &hueGreenButton;
        appData.hueBlueButton = &hueBlueButton;

    // Inicjowanie interfejsu
        gtk_init (&argc, &argv);

    // Ładowanie GtkBuilder z pliku ze schematem interfejsu
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_file (builder, UI_FILE, &error) == 0)
        {
            g_printerr ("Błąd przy ładowaniu pliku: %s\n", error->message);
            g_clear_error (&error);
            return 1;
        }

    // Przypisywanie obiektów interfejsu do wskaźników na nie
        mainWindow = gtk_builder_get_object (builder, "mainWindow");
        chooseFileButton = (GtkFileChooserButton *)gtk_builder_get_object (builder, "chooseFileButton");
        applyButton = gtk_builder_get_object (builder, "applyButton");
        resetButton = gtk_builder_get_object (builder, "resetButton");
        changesButton = gtk_builder_get_object (builder, "changesButton");
        exportButton = gtk_builder_get_object (builder, "exportButton");

        *appData.brightnessButton = gtk_builder_get_object (builder, "brightnessButton");
        *appData.contrastButton = gtk_builder_get_object (builder, "contrastButton");
        *appData.exposureButton = gtk_builder_get_object (builder, "exposureButton");
        *appData.saturationButton = gtk_builder_get_object (builder, "saturationButton");
        *appData.gammaButton = gtk_builder_get_object (builder, "gammaButton");
        *appData.temperatureButton = gtk_builder_get_object (builder, "temperatureButton");
        *appData.hueRedButton = gtk_builder_get_object (builder, "hueRedButton");
        *appData.hueGreenButton = gtk_builder_get_object (builder, "hueGreenButton");
        *appData.hueBlueButton = gtk_builder_get_object (builder, "hueBlueButton");

    // Ustawianie nasłuchu sygnałów
        g_signal_connect (mainWindow, "destroy", G_CALLBACK(closeWindow), &appData);
        g_signal_connect (chooseFileButton, "file-set", G_CALLBACK(loadImage), &appData);
        g_signal_connect (applyButton, "clicked", G_CALLBACK(applySettings), &appData);
        g_signal_connect (resetButton, "clicked", G_CALLBACK(resetSettings), &appData);
        g_signal_connect (changesButton, "pressed", G_CALLBACK(displayOriginalImage), &appData);
        g_signal_connect (changesButton, "released", G_CALLBACK(displayOriginalImage), &appData);
        g_signal_connect (exportButton, "clicked", G_CALLBACK(exportFile), &appData);

        g_signal_connect (brightnessButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.brightness);
        g_signal_connect (contrastButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.contrast);
        g_signal_connect (exposureButton, "value-changed", G_CALLBACK(saveButtonValueFloat), &userSettings.exposure);
        g_signal_connect (saturationButton, "value-changed", G_CALLBACK(saveButtonValueFloat), &userSettings.saturation);
        g_signal_connect (gammaButton, "value-changed", G_CALLBACK(saveButtonValueFloat), &userSettings.gamma);
        g_signal_connect (temperatureButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.colorTemperature);
        g_signal_connect (hueRedButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.hue[RED]);
        g_signal_connect (hueGreenButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.hue[GREEN]);
        g_signal_connect (hueBlueButton, "value-changed", G_CALLBACK(saveButtonValueInt), &userSettings.hue[BLUE]);

    // Otwieranie i wyświetlanie zdjęcia jeśli zostało podane jako argument
        if(imageName.size() > 0)
        {
        // Wyświetlanie zmiennych podanych jako argumenty na przyciskach
            gtk_file_chooser_set_filename((GtkFileChooser *)chooseFileButton, imageName.c_str());
            refreshButtonLabels(&appData);

        // Wyświetlanie zdjęcia z zastosowanymi ustawieniami
            loadImage((GtkWidget *)chooseFileButton, &appData);
            applySettings(NULL, &appData);
        }


        gtk_main ();
    }

    return 0;
}