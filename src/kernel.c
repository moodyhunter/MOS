const char *const VIDEO_MEMORY = (char *) 0xB8000;

void print_string(const char *str)
{
    char *video = (char *) VIDEO_MEMORY;
    for (; *str; str++)
    {
        char c = *str;
        *video = c;
        *(video + 1) = 0x0f;

        video += 2;
    }
}

void start_kernel(void)
{
    const char *kernel_name = "Hello from the MOS kernel!";
    print_string(kernel_name);

    // char *video_memory = (char *) 0xb8000;
    // *video_memory = 'X';
    while (1)
        ;
}
