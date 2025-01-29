gcc dystrybuatornia.c -o dytrybutornia
gcc magazyn.c -o magazyn
./dytrybutornia $((RANDOM)) 512 30 40 50 & ./magazyn conf_m1.txt $((RANDOM)) & ./magazyn conf_m2.txt $((RANDOM)) & ./magazyn conf_m3.txt $((RANDOM))
