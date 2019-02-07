# README (in Italian) #

## Istruzioni per utilizzare il Subtracker ##

### Compilare ###
* Servono (un po' di) questi pacchetti: `qt5-default build-essential cmake qt5-qmake qtbase5-dev-tools libturbojpeg0-dev pkgconfig libboost-all-dev`
* Dare il comando `mkdir my_opencv && cd my_opencv`
* Clonare i repo `opencv` e `opencv-contrib`
* Creare la cartella `mkdir opencv_install`
* Dare `cd opencv && mkdir build && cd build`
* Usare il comando che c'è nel file di questo repo, `opencv_cmake_line.txt`
* Dare `make && make install`
* Esportare la variabile `LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/my_opencv/opencv_install/lib`.
* Far conoscere a pkg-config il file `opencv_install/lib/pkgconfig/opencv.pc` (tipo copiandolo in `/usr/share/pkgconfig`)
* Andare nella cartella `qt/Subtracker` e dare `qmake` (se si lamenta, dare `export QT_SELECT=qt5`)
* Finalmente dare `make`
* Clonare il repo `streaming`, installarne le dipendenze come scritto nel README e dare `make`

### Usare ###
* Collegare la webcam giusta via USB.
* Utilizzare il programma `streaming/v4l2_source.c` per interfacciarsi con la webcam e scrivere frame in formato JPEG su standard output.
Opzioni utili sono: `-d DEVICE` (per indicare la webcam), `-a` (si riavvia automaticamente ad ogni crash, per esempio se la webcam viene scollegata e ricollegata), `-f` (forza la risoluzione 640x480 e formato MPEG).
* Mandare i frame in pipe a `streaming/monitor_tee.py`, il quale ascolta (e manda i frame) su una porta TCP (default 2204) e opzionalmente fa il dump sull'hard disk per l'archiviazione (serve tanta memoria, ordine dei 500 GB).
L'archiviazione dei frame su disco è attivata/disattivata premendo il tasto `r` (crea un file da 1 GB ogni 3 minuti circa).
Riassumendo, un comando ragionevole è `./v4l2_source -d /dev/video0 -a -f | ./monitor_tee.py 640 480`.
Opzioni utili di `monitor_tee.py` sono: `-p PORT` (per indicare la porta), `-d DIRECTORY` (cartella per l'archiviazione).
* Eseguire `Subtracker` (che si connette a localhost sulla porta 2204). 
* Redirigere l'output di `Subtracker` a `web/insert.py` per inserire i dati nel database (controllare di avere più di 1 GB libero).
Comando completo: `./Subtracker | (cd ~/subtracker/web/; ./insert.py)`
* TODO: servizio che rende disponibili i dati ai client HTTP.
