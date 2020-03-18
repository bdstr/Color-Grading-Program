# Color Grading Program, Marcin Kloczkowski s20489

## Opis
Moim projektem jest program pozwalający na podstawowy Color Grading, czyli edycję zdjęć pod kątem kolorów. Posiada funkcjonalności wypisane poniżej, dodatkowo oprócz interfejsu graficznego obsługuje także edycję zdjęcia jedynie z linii poleceń przez użycie odpowiednich flag. Pliki muszą być w formacie PNG, JPG lub TIFF, w innym wypadku program nic nie wyświetli.
Program był pisany oraz testowany na Fedorze 31 ze środowiskiem graficznym Gnome 3.34.3 oraz zainstalowanymi wszelkimi potrzebnymi zależnościami.

Projekt znajduje się w pliku main.cpp, pliki w katalogu "other" to poprzednie iteracje projektu (milestony), które zostawiłem w ramach backupu, biblioteka OpenCV oraz "symulacja" logiki stojącej za funkcją dostosowywania cieni, tonów średnich i prześwietleń na zdjęciach, którą wymyśliłem. W katalogu resources jest tylko plik ze schematem interfejsu, który jest wczytywany przez program.

## Dostępne funkcjonalności

| Funkcjonalność				| Flaga | Wartości	|
|:----------------------------------------------|:-----:|:-------------:|
| Jasność (brightness)				| -b 	| [-255 - 255]	|
| Kontrast (contrast)				| -c 	| [-255 - 255]	|
| Ekspozycja (exposure)			| -e 	| [-4.0 - 4.0]	|
| Saturacja (saturation)			| -s 	| [0.0 - 4.0]	|
| Temperatura kolorów (color temperature)	| -t 	| [-255 - 255]	|
| Zmiana odcienia (hue shifting)		| -hr<br/> -hg<br/> -hb | [-255 - 255]<br/>(odpowiednio czerwony, zielony i niebieski)
| Lift						| -l 	| [-255 - 255]	|
| Gamma					| -g 	| [0.0 - 4.0]	|
| Gain						| -gn 	| [0.0 - 2.0]	|
| Shadows					| -sh 	| [-1.0 - 1.0]	|
| Midtones					| -md 	| [-1.0 - 1.0]	|
| Highlights					| -hl 	| [-1.0 - 1.0]	|
| Zapis do pliku				| -o 	| [ścieżka]	|

<br/>
Np. `./Color\ Grading\ Program wejscie.jpg -c 15 -s 1.2 -sh -0.9 -o wyjscie.jpg`
wczyta plik "wejscie.jpg", zmieni jego kontrast, saturację, cienie i zapisze to w pliku "wyjscie.jpg" bez uruchamiania interfejsu


## Wymagane biblioteki
Program do działania wymaga bibliotek:

* OpenCV 4.1.1
* GTK+ 2.0

## Instalacja biblioteki OpenCV
Należy postępować zgodnie z tym linkiem:
<https://docs.opencv.org/4.0.0/d7/d9f/tutorial_linux_install.html>
używając tego polecenia cmake:
`cmake -D OPENCV_GENERATE_PKGCONFIG:BOOL="1" ..`

## Kompilacja projektu
Polecenie kompilacji:
```g++ main.cpp -Wall -Wextra `pkg-config opencv4 gtk+-2.0 --cflags --libs` -o "Color Grading Program"```

