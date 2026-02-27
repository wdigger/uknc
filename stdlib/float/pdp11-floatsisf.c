void __floatsisf(float *result, int _value) {
    unsigned int value;
    unsigned int sign = 0;
    
    __asm__ __volatile__ ("mov r1, %0" : "=r" (value));
    if (value == 0) 
    {
        ((unsigned int*)result)[0] = 0;
        ((unsigned int*)result)[1] = 0;
        return;
    }

    if ((int)value < 0) 
    {
        sign = 0100000;
        value = -((int)value);
    }

    unsigned int exp = 044000; 

    while ((value & 0100000) == 0) 
    {
        value <<= 1;
        exp -= 0200;
    }

    unsigned int high = ((value >> 8) & 0177) | exp | sign;

    ((unsigned int*)result)[0] = high;
    ((unsigned int*)result)[1] = 0;
}
