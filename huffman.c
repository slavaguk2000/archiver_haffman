#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define PATH_LEN_MAX 256
#define nullptr NULL

#define COMPRESS 1
#define DECOMPRESS 2

struct Node_word{
    struct Node_word* left;
    struct Node_word* right;
    uint8_t symbols[256];
    int count;
    int frequensy;
};

void add(struct Node_word* node, uint8_t new_symbol, int frequensy)
{
    for (int i = 0; i < node->count; i++)
        if (node->symbols[i] == new_symbol) return;
    node->symbols[node->count] = new_symbol;
    node->count++;
    node->frequensy += frequensy;
}

void init_Node(struct Node_word* node, uint8_t symbol, int frequensy)
{
    node->count = 0; 
    for (int i = node->count; i < 256; i++)
        node->symbols[i] = 0;    
    node->symbols[node->count] = symbol;
    node->frequensy = frequensy;
    node->count++;
    node->left = nullptr;
    node->right = nullptr;
}
void init_Node_from_Nodes(struct Node_word* node, struct Node_word* first, struct Node_word* second)
{
    node->count = first->count;
    for (int i = 0; i < node->count; i++){
        node->symbols[i] = first->symbols[i];
    }
    for (int i = 0; i < second->count; i++){
        node->symbols[node->count++] = second->symbols[i];
    }
    node->frequensy = first->frequensy + second->frequensy;
    node->left = first;
    node->right = second;
}

struct SortedStack{
    struct Node_word* array[256];
    int count;
};
void push(struct SortedStack* stack, struct Node_word* node)
{
    if (stack->count < 256){
        int i;
        for(i = stack->count; i > 0 && node->frequensy > stack->array[i - 1]->frequensy; i--)
            stack->array[i] = stack->array[i-1];
        stack->array[i] = node;
        stack->count++;
    }
}
struct Node_word* pop(struct SortedStack* stack)
{
    if (!stack->count) return nullptr;
    stack->count--;
    return stack->array[stack->count];
}
struct Node_word* getHead(struct SortedStack* stack)
{
    if (!stack->count) return nullptr;
    return stack->array[stack->count-1];
}

struct Code{
    uint64_t code;
    uint8_t  size;
};

int process_args(const int argc, char **argv, char *ifile_path, char *ofile_path)
{
    if (argc < 4) return 0;//count error
    if (strlen(argv[2]) != 2 || argv[2][0] != '-') return 0;//flag error
    strcpy(ifile_path, argv[1]);
    strcpy(ofile_path, argv[3]);
    switch (argv[2][1]){
        case 'c': return COMPRESS;
        case 'x': return DECOMPRESS;
        default: return 0;//flag error
    } 
}

long get_file_length(FILE* file)
{
    fseek(file, 0, SEEK_END);
    long end = ftell(file);
    fseek(file, 0, SEEK_SET);
    return end - ftell(file);
}

uint8_t* get_memory_block_from_file(FILE* file, long* size)
{
    *size = get_file_length(file);
    uint8_t* mem_block = (uint8_t*)malloc(*size);
    if (!mem_block) return nullptr; //allocation error
    if(fread(mem_block, sizeof(uint8_t), *size, file) < *size) {
        free(mem_block);
        return nullptr; //read error
    }
    return mem_block;
} 

void init_table(const uint8_t* source_memory, const int source_size, int* table)
{
    for (int i = 0; i < source_size; i++)
        table[source_memory[i]]++;
}

struct SortedStack get_stack(int* table)
{
    struct SortedStack stack;
    stack.count = 0;
    for (int i = 0; i < 256; i++){
        int max_freq = 0;
        uint8_t max_freq_index = 0;
        for (int j = 0; j < 256; j++){
            if (table[j] > max_freq){
                max_freq = table[j];
                max_freq_index = j;
            }
        }
        if (!max_freq) break;
        struct Node_word* node = (struct Node_word*)malloc(sizeof(struct Node_word));
        init_Node(node, max_freq_index, max_freq);
        push(&stack, node);
        table[max_freq_index] = 0;
    }
    return stack;
}

struct Node_word* get_tree(int* table)
{
    struct SortedStack stack = get_stack(table);
    if(!stack.count) return nullptr;
    while(stack.count > 1){
        struct Node_word* first = pop(&stack);
        struct Node_word* second = pop(&stack);
        struct Node_word* newNode = (struct Node_word*)malloc(sizeof(struct Node_word)); 
        init_Node_from_Nodes(newNode, first, second);
        push(&stack, newNode);
    }
    return getHead(&stack);
}

int write_in_file_bits(FILE* file, const uint64_t code, const uint8_t count_bits)
{
    static uint64_t buffer = 0;
    static int8_t padding_count = 64;
    if(count_bits && count_bits <= 64)
    {
        padding_count -= count_bits;
        if (padding_count <= 0){
            buffer |= code >> -padding_count;    
            if(!fwrite(&buffer, sizeof(uint64_t), 1, file)) return -1;
            buffer = 0;
            padding_count += 64;    
        }
        if(padding_count != 64) buffer |= code << padding_count; 
    }
    return padding_count;
}

int8_t read_from_file_bits(FILE* file, uint64_t* code, const uint8_t count_bits)
{
    static uint64_t buffer = 0;
    static int8_t unreaded = 0; 
    if(!count_bits || count_bits > 64) return unreaded;
    *code = 0;
    unreaded -= count_bits;
    uint64_t mask = ((uint64_t)-1) >> (64 - count_bits);
    if (unreaded < 0)
    {
        *code = buffer << (-unreaded) & mask; 
        if(!fread(&buffer, sizeof(uint64_t), 1, file)) return -1;
        unreaded += 64;
    }
    *code |= buffer >> unreaded & mask;
    return unreaded;
}

int initDictionary(const struct Node_word *node, struct Code* dictionary, uint64_t code, uint8_t count_bits)
{
    if(count_bits == 64) return 1;
    if(!node->left && !node->right){
        if(node->count != 1) return 4;
        dictionary[node->symbols[0]].code = code;
        dictionary[node->symbols[0]].size = count_bits;
        return 0;
    } 
    code = code << 1;
    count_bits++;
    if (node->left && node->right) 
        return initDictionary(node->left, dictionary, code, count_bits) | 
            initDictionary(node->right, dictionary, code|1, count_bits);
    return 2;
}

int write_dictionary_in_file(const struct Code* dictionary, FILE* file)
{
    uint8_t padding_count = 0;
    if(!fwrite(&padding_count, sizeof(uint8_t), 1, file)) return -1;
    for(int i = 0; i < 256; i++){
        if( write_in_file_bits(file, (uint64_t)dictionary[i].size, 8) == -1 ||
            (padding_count = write_in_file_bits(file, dictionary[i].code, dictionary[i].size)) == -1) return -1;
    }
    return padding_count;
}

uint8_t read_dictionary_from_file(struct Code* dictionary, FILE* file) 
{
    uint8_t padding_count = 0;
    if(!fread(&padding_count, sizeof(uint8_t), 1, file)) return -1;
    uint64_t size;
    for(int i = 0; i < 256; i++){
        if (read_from_file_bits(file, &size, 8) == -1) return -2;
        dictionary[i].size = (uint8_t)size;
        if (read_from_file_bits(file, &dictionary[i].code, dictionary[i].size) == -1) return -3;
    }
    return padding_count;
}

int write_file_info(const struct Code* dictionary, FILE* file, const uint8_t* source_memory, const int source_size)
{
    int padding;
    for (int i = 0; i < source_size; i++){
        if((padding = write_in_file_bits(file, dictionary[source_memory[i]].code, dictionary[source_memory[i]].size)) == -1) return -1;
    }
    return padding;
}

long get_remain_count(const uint8_t padding_count, FILE* file)
{
    uint64_t code;
    int8_t unreaded = read_from_file_bits(file, &code, 0);
    fpos_t position;
    fgetpos(file, &position);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fsetpos(file, &position);
    length -= ftell(file);
    return unreaded + length*8 - padding_count;
}

uint64_t* get_compressed_memory_block(FILE* file, long count_bits)//verified
{
    uint64_t* info = (uint64_t*)malloc(((count_bits+63)/64)*sizeof(uint64_t));
    if(!info) return nullptr;
    for (int i = 0; count_bits; i++){
        uint8_t count_bits_to_read = count_bits > 64? 64: count_bits;
        count_bits -= count_bits_to_read;
        if(read_from_file_bits(file, &info[i], count_bits_to_read) < 0) {
            free(info);
            return nullptr;
        }
        info[i] = info[i] << (64 - count_bits_to_read);
    }
    return info;
}

int16_t check_in_dictionary(const struct Code* dictionary, const uint64_t code, const uint8_t size)//verified
{
    for(int16_t i = 0; i < 256; i++){
        if(dictionary[i].size == size && dictionary[i].code == code) {
            return i;
        }
    }
    return -1;
}

int create_decompress_file(uint64_t* block_info, const struct Code* dictionary, FILE* file, long count_bits)//verified
{
    long readed_bits = 0;
    long exist = count_bits - readed_bits;
    while (readed_bits < count_bits){
        uint64_t code = 0;
        uint8_t length_code = 0;
        uint16_t answer_from_dictionary;
        do{
            code = code << 1;
            uint64_t source_code = block_info[readed_bits/64];
            uint8_t shift_count = (uint8_t)(((exist) >63?63:exist) - readed_bits%64);
            uint64_t mask =  (((uint64_t)1) << shift_count);
            uint64_t new_bit = !!(source_code & mask); 
            code |= new_bit;    
            length_code++;
            readed_bits++;
            if(length_code > 64) return -1;
            answer_from_dictionary = check_in_dictionary(dictionary, code, length_code);
        }while(answer_from_dictionary == (uint16_t)-1);
        uint8_t byte_to_write = (uint8_t)answer_from_dictionary;
        fwrite(&byte_to_write, sizeof(uint8_t), 1, file);
    }
    return 0;
}

int write_decompress_file(const uint8_t padding_count, const struct Code* dictionary, FILE* ifile, FILE* ofile)
{
    long count_bits = get_remain_count(padding_count, ifile);
    uint64_t* block_info = get_compressed_memory_block(ifile, count_bits);
    if(!block_info) return -1;
    int error_code = create_decompress_file(block_info, dictionary, ofile, count_bits);
    free(block_info);
    return error_code;
}

int write_padding(FILE* file, const uint8_t padding)
{
    fseek(file, 0, SEEK_SET);
    if(!fwrite(&padding, sizeof(uint8_t), 1, file)) return -1;
    return 0;
}

int write_compress_file(const uint8_t* source_memory, const int source_size, FILE* file)
{
    int table[256] = {0};
    init_table(source_memory, source_size, table);
    struct Node_word* root = get_tree(table);
    struct Code dictionary[256] = {0};
    if(initDictionary(root, dictionary, 0,0)) return 1;
    if(write_dictionary_in_file(dictionary, file) < 0) return -1;
    int padding_count = write_file_info(dictionary, file, source_memory, source_size);
    if(write_in_file_bits(file, 0, padding_count) != 64) return -1;
    if(write_padding(file, padding_count) < 0) return -1;  
    return 0;
}

int decompress_file(FILE* ifile, FILE* ofile)
{
    struct Code dictionary[256] = {0};
    uint8_t padding_count = read_dictionary_from_file(dictionary, ifile);
    //we can store byte value as one more field, sort dictionary and find with binary search, but I am very lazy to do it
    return write_decompress_file(padding_count, dictionary, ifile, ofile);
}

int compress_file(FILE *ifile, FILE *ofile)
{
    long source_size;
    uint8_t* memory_block = get_memory_block_from_file(ifile, &source_size);
    if(!memory_block) return 1;
    int error_code = write_compress_file(memory_block, source_size, ofile);
    free(memory_block);
    return error_code;
}

int main(int argc, char* argv[])
{
    char ifile_path[PATH_LEN_MAX], ofile_path[PATH_LEN_MAX]; 
    int choise = process_args(argc, argv, ifile_path, ofile_path); 
    if(!choise) return 1;
    FILE* ofile = fopen(ofile_path, "wb");
    if(!ofile) return 2;//file_open error
    FILE* ifile = fopen(ifile_path, "rb");
    if(ifile){    
        switch(choise){
            case COMPRESS:
                compress_file(ifile, ofile);
                break;
            case DECOMPRESS:
                decompress_file(ifile, ofile);
                break;
        }
        fflush(ifile);
        fclose(ifile);
    } 
    fflush(ofile);
    fclose(ofile);
    return 0;
}