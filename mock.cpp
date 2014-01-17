#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
using namespace std;
using namespace chrono;

int main() {
    int count = 0;
    while(++count) {
        this_thread::sleep_for(milliseconds(8));
        auto now = high_resolution_clock::now().time_since_epoch();
        // angoli:      centesimi di radiante
        // posizione:   millesimi di unità subotto
        // tempo:       millesimi di secondo
        auto ms = duration_cast<milliseconds>(now).count();
        int palla_x = sin(float(count) / 200) * 400;
        int palla_y = cos(float(count) / 200) * 400;
        cout << "palla " << palla_x << " " << palla_y << " " << ms << endl;
        if(rand()%400 == 0) {
            cout << "tocco " << palla_x << " " << palla_y << " " << ms << endl;
        }
        int stecca_pos[8];
        int stecca_rot[8];
        for(int i=0; i<8; i++) {
            stecca_pos[i] = sin(float(count + i*30) / 300) * (40-4*i);
            stecca_rot[i] = (4*count + i*40) % 628;
            cout << "stecca " << i << " "<< stecca_pos[i] << " "
                 << stecca_rot[i] << " " << ms << endl;
        }
    }
}
