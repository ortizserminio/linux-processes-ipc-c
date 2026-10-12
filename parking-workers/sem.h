//Creamos un semáforo con un valor inicial
int sCreate(char seed, int value);

//Operaciones de incremento y decremento del semáforo
void sWait(int semID);
void sSignal(int semID);
void sDestroy(int semID);