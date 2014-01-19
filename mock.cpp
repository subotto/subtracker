#include <cstdio>
#include <chrono>
#include <thread>
#include <cmath>
using namespace std;
using namespace chrono;

/**
 *  Un "evento" per riga, dove gli eventi sono:
 *  - palla   la posizione della pallina (dando la coordinata x e y)
 *            Parametri: x y
 *  - tocco   è avvenuto un tocco della pallina in una certa posizione
 *            Parametri: x y
 *  - stecca  la posizione dell'i-esima stecca (coordinata y e angolo)
 *            Parametri: i y angolo
 *
 *  Ogni evento è nel formato "nome_evento timestamp parametri"
 *
 *  Le unità di misura usate sono:
 *  - angoli      radianti
 *  - posizione   metri
 *  - tempo       secondi dal tempo di riferimento
 */

int main() {
    int count = 0;
    auto ref = high_resolution_clock::now();
    while(++count) {
        this_thread::sleep_for(milliseconds(8));
        auto now = high_resolution_clock::now();
        float ms = float(duration_cast<milliseconds>(now - ref).count()) / 1000;
        float palla_x = sin(float(count) / 200) * 0.4;
        float palla_y = cos(float(count) / 200) * 0.4;
        printf("palla %.3f %.3f %.3f\n", ms, palla_x, palla_y);
        if(rand()%400 == 0) {
            printf("tocco %.3f %.3f %.3f\n", ms, palla_x, palla_y);
        }
        float stecca_pos[8];
        float stecca_rot[8];
        for(int i=0; i<8; i++) {
            stecca_pos[i] = sin(float(count + i*30) / 300) * (0.4-0.04*i);
            stecca_rot[i] = float((count + i*40) % 628) / 100;
            printf("stecca %.3f %d %.3f %.3f\n", ms, i, stecca_pos[i], stecca_rot[i]);
        }
    }
}
