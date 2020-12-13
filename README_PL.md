
Program do przetwarzania danych wysyłanych przez stację pogody Meteo SP73.
Ta stacja pogody jest prawie identyczna z wyglądu do stacji pogody GreenBlue
GB522.
Stacje Meteo SP73 oraz GreenBlue GB522 wyglądają identycznie, poza logotypami
marki.

Program działa pod kontrolą GNU/Linuksa, w tym także DD-WRT. Powinien zadziałać
bez problemu na Raspberry Pi. Nie był testowany na OpenWRT, jednak dostosowanie
go do tego środowiska powinno być proste.

Przeniesienie na Windowsa (przy użyciu MinGW) wymagałoby pewnej ilości pracy,
jednak z użyciem biblioteki Gnulib też mogłoby być stosunkowo proste.

Program jest oparty na pracy wykonanej przez Pana Łukasza Kalamłackiego, który
wykonał analizę wsteczną protokołu stacji:
http://kalamlacki.eu/sp73.php

Raporty z używania programu mile widziane - przez bug report na GitHubie lub
mail na mat.jonczyk w domenie o2.pl.

Formaty wyjścia programu (JSON, CSV) mogą się zmienić w przyszłej wersji.
