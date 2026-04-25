/* Provide __dso_handle for static builds */
void *__dso_handle = 0;

/* Constructor/destructor support */
typedef void (*ctor_func)(void);
extern ctor_func __init_array_start[];
extern ctor_func __init_array_end[];
extern ctor_func __fini_array_start[];
extern ctor_func __fini_array_end[];

/* Entry point - should be provided by the OS */
void _start(void);
void _start(void) {
    /* Call global constructors (initializes stdin, stdout, stderr) */
    for (ctor_func *ctor = __init_array_start; ctor < __init_array_end; ctor++) {
        (*ctor)();
    }
    
    /* Call main with argc/argv - OS-specific setup needed */
    int argc = 0;
    char** argv = (void*)0;

    extern int main(int argc, char** argv);
    int ret = main(argc, argv);
    
    extern void exit(int status);
    exit(ret);
    
    for(;;); 
}
