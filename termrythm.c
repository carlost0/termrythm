#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h> 
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

struct termios oldt;
char kb_input = 0;
char input = 0;

struct parser_node {
    char key;
    char *val;
    struct parser_node *next;
};

void disable_canonical_input() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);      // get current terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);    // disable canonical mode & echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void enable_canonical_input() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // restore old setting
}

void* get_keyboard_input() {
    read(STDIN_FILENO, &kb_input, 1);  // Read one character
    return NULL;
}

char* trim_str(char *str) {
    if (str == NULL) return NULL;
 
    char *start;
    char* end;
    start = str;

    while (isspace((unsigned char)*start)) {
        start++;
    }
 
    if (*start == '\0') {
        *str = '\0'; 
        return str;
    }
 
    end = start;
    while (*end != '\0') {
        end++;
    }
    end--; 
 
    while (end >= start && isspace((unsigned char)*end)) {
        end--;
    }
    end++;  
    *end = '\0';  
 
    return start;
}

char* file_to_str(char *filename) {
    FILE *file;
    file = fopen(filename, "rb");

    if (!file) {
        perror("unable to open file :(\nerror");
        return NULL;
    }

    fseek(file, 0, SEEK_END);

    size_t file_size;
    file_size = ftell(file);

    fseek(file, 0, SEEK_SET);

    char *buf;
    buf = (char*)malloc(file_size * sizeof(char) + 1);

    if (!buf) {
        perror("can't allocate memory for file buffer :(\nerror");
        fclose(file);
        return NULL;
    }

    fread(buf, 1, file_size, file);
    buf[file_size + 1] = '\0';

    fclose(file);

    return buf;
}

int push(struct parser_node *head, char key, char *val) {
    struct parser_node *current;
    current = head;

    while (current->next != NULL)
        current = current->next;

    current->next = (struct parser_node *) malloc(sizeof(struct parser_node));

    if (current->next == NULL) {
        printf("unable to allocate memroy :(\nerror");
        exit(1);
    }

    current->next->key = key;
    current->next->val = val;
    current->next->next = NULL;

    return 0;
}

struct parser_node *parse_config(char *conf) {
    struct parser_node *head = (struct parser_node *) malloc (sizeof(struct parser_node));
    char *str;

    if (head == NULL) {
        printf("can't allocate memory");
        exit(1);
    }

    head->next = NULL;

    str = trim_str(strtok(conf, " :;"));

    while (str != NULL) {
        push(head, *str, trim_str(strtok(NULL, " :;")));
        str = trim_str(strtok(NULL, " :;"));
    }

    return head->next == NULL ? NULL : head->next;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage:\n"                             \
                   "\ttermrythm [config-file.tr]\n\n" \
               "Config file syntax:\n"                \
                   "\tkey: file.wav;\n\n"                \
               "Example:\n"                           \
                   "\tw: kick.wav;\n"                  \
                   "\te: snare.wav;\n\n"                 \
               "Limitations:\n"                       \
                   "\t1. The keys may only be a lower case letter of the latin alphabet (ASCII 97-122) \n" \
                   "\t2. The filenames, including the extension, has to be less than 100 characters long\n");
        return 0;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)) {
        printf("can't initialize mixer\n");
        return 1;
    }

    char *conf = file_to_str(argv[1]);

    if (conf == NULL) {
        return 1;
    }
    Mix_Chunk *sample_chunks[26] = {0};

    struct parser_node *sample_node = parse_config(conf);

    if (sample_node == NULL) {
        printf("can't parse config file");
        return 1;
    }
    printf("press '.' to exit\n");
    printf("press ',' to halt all audio\n");
    while (sample_node->next != NULL) {
        printf("%c: %s\n", sample_node->key, sample_node->val);
        sample_chunks[sample_node->key - 97] = Mix_LoadWAV(sample_node->val);
        sample_node = sample_node->next;
    }

    disable_canonical_input();

    while (input != '.') {
        get_keyboard_input();
        input = kb_input;
        kb_input = 0;
        if (input == ',')
            Mix_HaltChannel(-1);
        if (input < 97)
            continue;
        if (sample_chunks[input - 97] != 0)
            Mix_PlayChannel(-1, sample_chunks[input - 97], 0);
    }

    enable_canonical_input();
    free(conf);

    printf("stopping...\n");
    SDL_Delay(1000);

    struct parser_node *tmp;
    while (sample_node->next != NULL) {
        tmp = sample_node;
        sample_node = sample_node->next;
        free(tmp);
    }

    for (int i = 0; i < 26; ++i)
        Mix_FreeChunk(sample_chunks[i]);

    Mix_CloseAudio();
    SDL_Quit();
}
