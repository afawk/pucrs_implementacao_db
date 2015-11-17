
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/*
SEEK_SET    Beginning of file
SEEK_CUR    Current position of the file pointer
SEEK_END    End of file *
 */

void carrega_fat_struct();
void carrega_dir_struct(int num_cluster);
void init();
void load();
void get_file_positions_in_fat(int *vetor,int posicao_inicial);
void varrer_disco_por_setor();
int go_to_cluster(char *path);
int buscar_proximo_livre_na_fat();
void rm(char *path);
void rmdarnis(char *path);
void rmdir(char *path);
int count_free_pos_in_fat();
int buscar_proximo_livre_na_fat_para_escrever(int cluster_atual,int isBlocoFinal);
void write(char *path, char *text);
void cat(char *path);
void ls(char *parametro,char *path);
void mkdir_create(char *path,int isFile);



const char dir_separator[2] = "/";
/* Our structure */
/* tabela FAT, 1024 entradas de 32 bits */
uint32_t fat[1024];

/* cluster pro boot */
uint8_t cluster[4096];

/* entrada de diretorio, 32 bytes cada */
typedef struct {
    uint8_t filename[16]; // 16 bytes
    uint8_t attributes;   // 1 byte
    uint8_t reserved[7];  // 7 bytes
    uint32_t first_block; // 4 bytes
    uint32_t size;        // 4 bytes
} dir_entry;

/* diretorios (incluindo ROOT), 128 entradas de diretorio
com 32 bytes cada = 4096 bytes */
dir_entry root_dir_cluster[128];

FILE *ptr_myfile;

void carrega_fat_struct(){
    fseek(ptr_myfile, 4096 ,SEEK_SET);
    int counter;
    uint32_t value;
    for (counter = 0; counter < 1024;counter ++) {
        fread(&fat[counter], sizeof(4),1,ptr_myfile);
    }
}

void carrega_dir_struct(int num_cluster){

    dir_entry dir;

    fseek(ptr_myfile, 4096 * num_cluster,SEEK_SET);
    int counter;
    for (counter = 0; counter < 128;counter ++) {
        fread(&root_dir_cluster[counter], sizeof(dir),1,ptr_myfile);
        //root_dir_cluster[counter] = dir;
    }
}

void init()
{
    if (ptr_myfile)
    {
        fclose(ptr_myfile);
    }

    if(remove("fat.part") == 0){
        printf(">> Deletando arquivo fat\n");
    }

    printf(">> Inicializando fat.\n");

    ptr_myfile = fopen("fat.part","w+b");
    if (!ptr_myfile)
    {
        printf("Unable to open file!");
        return;
    }

    /* inicializa o boot */
    int i;
    for(i=0; i<4096; i++){
        cluster[i] = 0xa5;
    }

    /* inicializa a fat */
    fat[0] = 0xffffffff; // boot_sector
    fat[1] = 0xffffffff; //fat_sector
    fat[2] = 0xffffffff; //root_dir_cluster
    for(i=3; i<1024; i++){
        fat[i] = 0x00000000;
    }

    /* inicializa o root dir
        root dir é o array, já está inicializado nas váriaveis globais
    */
    dir_entry root_dir;

    int counter;

    /* grava todos clusters em disco */
    //ps: não sei se o segundo parametro está correto "size -- This is the size in bytes of each element to be written.".
    fwrite(cluster , 1 , sizeof(cluster) -1, ptr_myfile );
    fwrite(fat , 1 , sizeof(fat) , ptr_myfile );
    fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile );

    //restante dos 1021 setores de 4K
    for(i=0; i<4096; i++){
        cluster[i] = 0x00000000;
    }

    //preenche do setor 3 ao 1023 com 0x0(disco zerado)
    for(i=0; i<1021; i++){
        fwrite(cluster , 1 , sizeof(cluster) , ptr_myfile );
    }

    fflush(ptr_myfile);
}

void load()
{
    ptr_myfile=fopen("fat.part","r+b");

    if (!ptr_myfile){
        printf("Unable to open file!");
        return;
    }

    //monta root_dir
    carrega_dir_struct(2);
    carrega_fat_struct();
    printf("Load complete!\n");

}

//atribui para o vetor passado por referencia
//todas as posicoes da fat onde o texto do arquivo foi gravado(mais de 1 cluster quando o texto tem mais de 4kb)
void get_file_positions_in_fat(int *vetor,int posicao_inicial){
    int prox_pos;
    int i;
    int counter = 0;

    if(fat[posicao_inicial] == 0xffffffff)
        vetor[counter] = posicao_inicial;
    else
        vetor[counter] = fat[posicao_inicial];

    for (i = posicao_inicial+1; i < 1024; i++) {
        prox_pos = fat[posicao_inicial];
        if(prox_pos == 0xffffffff)
            break;
    }
    return;
}

void varrer_disco_por_setor(){
    FILE *fp;
    const char* str = "SETOR ";

    fp=fopen("fat_hexa.txt", "w+b");
    if(fp == NULL)
        return;

    int counter;

    int i;
    for(i=0; i<4096; i++){
        cluster[i] = 0x00000000;
    }

    fseek(ptr_myfile, 0,SEEK_SET); //fseek(FILE * stream, long int offset, int whence);
    for ( counter = 0; counter < 1024;counter ++) {
        //fseek(ptr_myfile, sizeof(cluster)*counter,SEEK_SET); //fseek(FILE * stream, long int offset, int whence);
        fread(&cluster, sizeof(cluster),1,ptr_myfile); //fread ( void * ptr, size_t size, size_t count, FILE * stream );

        fprintf(fp, "\n-----------%s %d--------------",str,counter+1);
        fprintf(fp, "Endereco de memoria inicial: %lu\n",sizeof(cluster)*counter);

        int j;
        for (j = 0; j < sizeof(cluster);j ++) {
            //printf("%x ", cluster[j]);
            fprintf(fp, "%x ", cluster[j]);
        }
    }
    fclose(fp);
}

//metodo que carrega em root_dir_cluster a estrutura de diretorios do ultimo diretorio indicado pelo path e retorna o numero do cluster
//se ultimo elemento do path for um arquivo, retornara o cluster a estrutura de diretorios do diretorio onde o arquivo esta.
int go_to_cluster(char *path){
    char *filename;
    char *token;
    int block_dir = 2;
    int achou=0;

    carrega_dir_struct(2);

    token = strtok(path, dir_separator);

    while( token != NULL ){
        int i;
        for(i = 0; i < 128; i++){
            if(strcmp((const char*) root_dir_cluster[i].filename,token)==0 && (int)root_dir_cluster[i].attributes == 0b00001000){
                achou = 1;
                block_dir = root_dir_cluster[i].first_block;
                break;
            }
        }

        carrega_dir_struct(block_dir);
        filename = token;
        token = strtok(NULL, dir_separator);
    }

    return block_dir;
}

int buscar_proximo_livre_na_fat(){

    int counter;
    int valor;
    int resultado = -1;
    fseek(ptr_myfile, 4096,SEEK_SET);
    for (counter = 0; counter < 1024;counter ++) {
        fread(&valor, sizeof(int),1,ptr_myfile);
        if(valor == 0x0){
            resultado = counter;
            fat[resultado] = 0xffffffff; //root_dir_cluster

            fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
            fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.
            break;
        }
    }
    fflush(ptr_myfile);
    return resultado;
}

void rmdarnis(char *path){
    char *filename;
    char *token;
    int block_dir_pai = 2;
    int block_dir_fat = 2;

    carrega_dir_struct(block_dir_pai);

    token = strtok(path, dir_separator);

    while( token != NULL ){
        int i;
        filename = token;
        token = strtok(NULL, dir_separator);

        //caminha na estrutura de diretorios
        for(i = 0; i < 128; i++){
            if(strcmp((const char*) root_dir_cluster[i].filename,filename) == 0){
                if (token != NULL)
                    block_dir_pai = root_dir_cluster[i].first_block;
                else
                    block_dir_fat = root_dir_cluster[i].first_block;
                break;
            }
        }

        if (token != NULL)
            carrega_dir_struct(block_dir_pai);
        else
            break;
    }
    int i;
    for(i = 0; i < 128; i++){
        if (strcmp((const char*) root_dir_cluster[i].filename,filename)==0){
            dir_entry new_dir;
            memset(&new_dir, 0, sizeof new_dir);
            root_dir_cluster[i] = new_dir;
            fseek(ptr_myfile, 4096*block_dir_pai,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a diretorio
            fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile ); //reescreve o setor da diretorio.

            do{
                if(fat[block_dir_fat] == 0xffffffff){
                    fat[block_dir_fat] = 0x0;
                    fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
                    fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.
                }else{
                    int aux = (int)fat[block_dir_fat];
                    fat[block_dir_fat] = 0x0;
                    block_dir_fat = aux;
                    fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
                    fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.
                }
            }while(fat[block_dir_fat] != 0xffffffff);
            fflush(ptr_myfile);
            break;
        }
    }
    return;
}

void rm(char *path){
    int achou = 0;
    int block_dir = 3;
    char *filename;
    char *token;
    int text_size = 0;

    token = strtok(path, dir_separator);
    filename  = token;
    block_dir = go_to_cluster(path);

    while( token != NULL ){
        filename = token;
        token = strtok(NULL, dir_separator);
    }

    int i;
    for(i = 0; i < 128; i++){
        if(strcmp((const char*) root_dir_cluster[i].filename,filename)==0 && (int)root_dir_cluster[i].attributes == 0b00000000){
            achou = 1;
            fseek(ptr_myfile, 4096 * block_dir,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para o diretorio

            block_dir = root_dir_cluster[i].first_block;

            dir_entry new_dir;
            memset(&new_dir, 0, sizeof new_dir);
            root_dir_cluster[i] = new_dir;

            fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile ); //reescreve o setor do diretorio.
            break;

        }
    }

    if(achou == 0){
        printf("Arquivo nao encontrado");
        return;
    }

    int vetor_posicoes_fat[(text_size/4096+1)];

    //pega todas as posicoes da fat ocupadas pelo arquivo
    get_file_positions_in_fat(vetor_posicoes_fat,block_dir);

    int length = sizeof(vetor_posicoes_fat)/sizeof(int);

    //libera os slots na fat ocupados pelo arquivo
    for (i = 0; i < length; i++) {
        fat[vetor_posicoes_fat[i]] = 0x0;
    }

    fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
    fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.
    return;
}

//remove a e
void rmdir(char *path){
    char *filename;
    char *token;
    int block_dir_pai = 2;
    int block_dir_fat = 2;

    carrega_dir_struct(block_dir_pai);

    token = strtok(path, dir_separator);

    if (token == NULL){
        printf("Nao faz besteira seu asno. Nao pode deletar o root_dir!.\n>>");
        return;
    }


    while( token != NULL ){
        int i;
        filename = token;
        token = strtok(NULL, dir_separator);

        //caminha na estrutura de diretorios
        for(i = 0; i < 128; i++){
            if(strcmp((const char*) root_dir_cluster[i].filename,filename) == 0){
                if (token != NULL)
                    block_dir_pai = root_dir_cluster[i].first_block;
                else
                    block_dir_fat = root_dir_cluster[i].first_block;
                break;
            }
        }

        if (token != NULL)
            carrega_dir_struct(block_dir_pai);
        else{
            carrega_dir_struct(block_dir_fat);
            for(i = 0; i < 128; i++){
                if (strcmp((const char*) root_dir_cluster[i].filename,"") != 0){
                    printf("Nao foi possivel remover este diretorio, pois ele possui diretorios/arquivos dentro dele.\n");
                    return;
                }
            }
            carrega_dir_struct(block_dir_pai);
            break;
        }
    }

    int i;

    for(i = 0; i < 128; i++){
        if (strcmp((const char*) root_dir_cluster[i].filename,filename)==0){
            dir_entry new_dir;
            memset(&new_dir, 0, sizeof new_dir);
            root_dir_cluster[i] = new_dir;
            fseek(ptr_myfile, 4096*block_dir_pai,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a diretorio
            fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile ); //reescreve o setor da diretorio.

            fat[block_dir_fat] = 0x0;
            fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
            fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.

            break;
        }
    }
    fflush(ptr_myfile);
    return;
}

int count_free_pos_in_fat(){
    int prox_pos;
    int i;
    int counter = 0;
    for (i = 0; i < 1024; i++) {
        if(fat[i] != 0x0){
            counter++;
        }
    }
    return counter;
}


int buscar_proximo_livre_na_fat_para_escrever(int cluster_atual,int isBlocoFinal){

    int counter;
    int resultado = -1;
    fseek(ptr_myfile, 4096,SEEK_SET);
    for (counter = cluster_atual; counter < 1024;counter ++) {
        if(fat[counter] == 0x0){
            resultado = counter;
            /*if (isBlocoFinal == 1)
                fat[cluster_atual] = 0xffffffff;
            else*/

            //cluster atual da fat aponta para o proximo cluster usado
            fat[cluster_atual] = counter;

            //ultimo cluster usado recebe -1
            fat[counter] = 0xffffffff;

            fseek(ptr_myfile, 4096,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a FAT
            fwrite(fat , 1 , sizeof(fat) , ptr_myfile ); //reescreve o setor da FAT.
            break;
        }
    }
    fflush(ptr_myfile);
    return resultado;
}

//escreve no arquivo contido no path, o conteudo de text
void write(char *path, char *text){
    char *filename;
    char *token;
    int block_dir = 2;
    int block_dir_aux = 2;
    int achou=0;

    if(text == NULL){
        printf("Numero de parametros incorreto.\nDigite \"man write\" para mais informacoes.\n");
        return;
    }
    uint32_t text_size = strlen(text);

    int reg_size = 0;

    int num_of_clusters = text_size/4096;

    if (text_size % 2 != 0 || num_of_clusters == 0){
        num_of_clusters++;
    }

    if (count_free_pos_in_fat() < num_of_clusters - 1){
        printf("Nao tem espaco suficiente em disco para gravar");
    }

    carrega_dir_struct(block_dir);

    token = strtok(path, dir_separator);
    while( token != NULL ){
        int i;
        for(i = 0; i < 128; i++){
            if(strcmp((const char*) root_dir_cluster[i].filename,token)==0){
                achou = 1;
                block_dir_aux = root_dir_cluster[i].first_block;
                break;
            }
        }

        filename = token;
        token = strtok(NULL, dir_separator);

        if(token != NULL){
            //nao chegou ao fim entao pega o proximo
            block_dir = block_dir_aux;
            carrega_dir_struct(block_dir);
            //como foi para o proximo nivel entao precisa achar de novo
            achou = 0;
        }else{
            if(achou == 1){
                carrega_dir_struct(block_dir); //monta a estrutura de diretorios padrao
                int counter;
                for (counter = 0; counter < 128;counter ++) {
                    if(strcmp((const char*) root_dir_cluster[counter].filename,filename) == 0){

                        //pega a ultima posicao escrita
                        reg_size = root_dir_cluster[i].size;

                        //novo tamanho do arquivo eh igual ao atual mais o texto que esta sendo adicionado
                        root_dir_cluster[i].size += text_size;

                    }
                }
                fseek(ptr_myfile, 4096 * block_dir,SEEK_SET);
                fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile );
            }
        }
    }
    if(achou == 0){
        printf("Arquivo nao encontrado.\n");
        return;
    }else{
        //variavel para quando o texto ultrapassar 4kb, pegar a ultima posicao escrita
        int last_index_text = 0;
        int counter = 0;
        do{
            fseek(ptr_myfile, 4096 * block_dir_aux,SEEK_SET);
            fread(&cluster, sizeof(cluster),1,ptr_myfile);
            int j;

            for (j = reg_size; j < 4096; j++) {
                if(j >= text_size+reg_size){
                    break;

                }
                cluster[j] = text[counter];
                counter++;
            }
            fseek(ptr_myfile, 4096 * block_dir_aux,SEEK_SET);
            fwrite(cluster , 1 , sizeof(cluster) , ptr_myfile );

            if (reg_size+ text_size >4096){
                //ultimo indice da string que foi escrito
                last_index_text = counter;
            }else{
                last_index_text = 0;
            }

            if( last_index_text != 0){
                //pega novo bloco para escrever se n teve espaco no bloco anterior
                block_dir_aux = buscar_proximo_livre_na_fat_para_escrever(block_dir_aux,1);
                //comeca a escrever do texto que faltou
                reg_size = 0;
                text_size = last_index_text;
            }
        }while(last_index_text != 0);


        }
    fflush(ptr_myfile);
    }


//imprime o conteudo do arquivo contido no path, se o arquivo existir
void cat(char *path){
    int achou = 0;
    int block_dir = 3;
    char *filename;
    char *token;
    char path_aux[strlen(path)];
    int text_size = 0;

    strcpy(path_aux, path);

    block_dir = go_to_cluster(path);

    token = strtok(path_aux, dir_separator);
    filename = token;

    while( token != NULL ){
        strcpy(filename, token);
        token = strtok(NULL, dir_separator);
    }

    int i;
    for(i = 0; i < 128; i++){
        if(strcmp((const char*) root_dir_cluster[i].filename,filename)==0 && (int)root_dir_cluster[i].attributes == 0b00000000){
            achou = 1;
            block_dir = root_dir_cluster[i].first_block;
            text_size = root_dir_cluster[i].size;
            break;
        }
    }

    if(achou == 0){
        printf("Arquivo nao encontrado.\n");
        return;
    }
    //vetor que recebera todas as posicoes da fat onde o texto do arquivo foi gravado(mais de 1 cluster quando o texto tem mais de 4kb)
    int vetor_posicoes_fat[(text_size/4096+1)];
    get_file_positions_in_fat(vetor_posicoes_fat,block_dir);

    int length = sizeof(vetor_posicoes_fat)/sizeof(int);
    for (i = 0; i < length; i++) {
        if(text_size < 4096){
            fseek(ptr_myfile, 4096 * vetor_posicoes_fat[i],SEEK_SET);

            //atribui 0x0 para todos os bytes do cluster(evita que fique sujeira)
            memset(&cluster, 0, sizeof cluster);

            fread(&cluster, sizeof(cluster),1,ptr_myfile);

            int j;
            for (j = 0; j < text_size; j++) {
                printf("%c",((char *)cluster)[j]);
            }
        }else{
            text_size -= 4096;
            fseek(ptr_myfile, 4096 * vetor_posicoes_fat[i],SEEK_SET);
            memset(&cluster, 0, sizeof cluster);
            fread(&cluster, sizeof(cluster),1,ptr_myfile);
            int j;
            for (j = 0; j < 4096; j++) {
                printf("%c",((char *)cluster)[j]);
            }
        }
    }
    printf("\n");
    return;
}



void ls(char *parametro,char *path){
    char *filename;
    char *token;
    const char *token_parameter_ls;
    int block_dir = 2;
    int achou=0;

    if (path == NULL){
        path = parametro;
        token_parameter_ls = "a";
    }

    token = strtok(path, dir_separator);

    if (parametro == 0x0){
        token_parameter_ls = "a";
    }else{
        token_parameter_ls = strtok(parametro,"-");
        if(strcmp(token_parameter_ls,"d") != 0 && strcmp(token_parameter_ls,"f") != 0 && strcmp(token_parameter_ls,"a") != 0){
            token_parameter_ls = "a";
        }
    }

    carrega_dir_struct(2);

    while( token != NULL ){
        int i;
        for(i = 0; i < 128; i++){
            if(strcmp((const char*) root_dir_cluster[i].filename,token)==0){
                achou = 1;
                block_dir = root_dir_cluster[i].first_block;
                break;
            }
        }

        carrega_dir_struct(block_dir);
        filename = token;
        token = strtok(NULL, dir_separator);
    }

    int i;

    for(i = 0; i < 128; i++){

        if(root_dir_cluster[i].first_block == 0x0)
                continue;

        //lista somente diretorios
        if(strcmp(token_parameter_ls, "d") == 0){
            if( (int)root_dir_cluster[i].attributes == 0b00001000){
                    printf("%s\n",root_dir_cluster[i].filename);
            }
        }
        //lista somente arquivos
        else if(strcmp(token_parameter_ls, "f") == 0){
                if( (int)root_dir_cluster[i].attributes == 0b00000000){
                        printf("%s\n",root_dir_cluster[i].filename);
                }
        }
        //lista tudo
        else if(strcmp(token_parameter_ls, "a") == 0 ){
            if( (int)root_dir_cluster[i].attributes == 0b00001000)
                    printf(" <DIR> %s\n",root_dir_cluster[i].filename);
            else
                    printf("%s\n",root_dir_cluster[i].filename);
        }
    }
}

void mkdir_create(char *path,int isFile){
    char *token;
    int achou;
    achou = 0;
    char *nome;
    int endereco_fat;
    int block_dir=2;

    token = strtok(path, dir_separator);

    carrega_dir_struct(block_dir);
    /* walk through other tokens */
    while( token != NULL ){
         //inicia o ponteiro do arquivo para o cluster reservado para a root_dir
        int counter;

        for (counter = 0; counter < 128;counter ++) {
            //nao encontrou e o
            if(strcmp((const char*) root_dir_cluster[counter].filename,token)==0){
                //diretorio
                nome = token;
                token = strtok(NULL, dir_separator);
                if (token == NULL){
                    if((isFile == 1 && (int)root_dir_cluster[counter].attributes == 0b00001000) ||(isFile == 0 && (int)root_dir_cluster[counter].attributes == 0b00000000)){
                        achou = 1;
                        block_dir = root_dir_cluster[counter].first_block;
                        break;
                    }
                }else{
                    block_dir = root_dir_cluster[counter].first_block;
                    carrega_dir_struct(block_dir);
                    counter = 0;
                }
            }
        }

        if(token != NULL){
            nome = token;
        }
        token = strtok(NULL, dir_separator);

        //não tem mais subdiretorios no path informado.
        if (token == NULL && nome != NULL){
            if(achou == 0){
                endereco_fat = buscar_proximo_livre_na_fat();
                if(endereco_fat != -1){
                    //fseek(ptr_myfile, 4096*block_dir,SEEK_CUR); //inicia o ponteiro do arquivo para o cluster reservado para a root_dir
                    carrega_dir_struct(block_dir); //monta a estrutura de diretorios padrao

                    for (counter = 0; counter < 128;counter ++) {

                        if(strcmp((const char*) root_dir_cluster[counter].filename,"")==0){
                            int i;
                            for (i = 0; i < strlen(nome); i++) {
                                root_dir_cluster[counter].filename[i] = nome[i];
                            }

                            root_dir_cluster[counter].size = 0;

                            if (isFile == 0) //is  file
                                root_dir_cluster[counter].attributes = 0b00000000;
                            else
                                root_dir_cluster[counter].attributes = 0b00001000;

                            root_dir_cluster[counter].first_block = endereco_fat;

                            fseek(ptr_myfile, 4096*block_dir,SEEK_SET); //inicia o ponteiro do arquivo para o cluster reservado para a root_dir
                            fwrite(root_dir_cluster , 1 , sizeof(root_dir_cluster) , ptr_myfile ); //reescreve o setor da bloco do dir.
                            break;
                        }
                    }
                }else{
                    printf("FAT cheia. Delete algum arquivo ou diretorio para criar um novo!\n");
                    return;
                }
            }else{
                printf("Diretorio ja existe! Tenta outro nome pocotó!\n");
                return;
            }//end if token == NULL
        }else{
            if(achou == 0){
                printf("Path invalido. Algum dos diretorios no path informado provavelmente nao existe.\nDigite um path valido pocotó!");
                return;
            }else{
                carrega_dir_struct(block_dir);
                achou = 0;
            }
        }  //end if achou == 0
    }//end while token != NULL

    fflush(ptr_myfile);
    return;
}

int main(){
    //create_file();
    char input[5000];
    char *comando;
    char *parametro1;
    char *parametro2;
    char *token;

    while(1){
        printf(">> ");
        scanf(" %[^\n]s",input);

        /* get the first token */

        comando = strtok(input, " ");
        parametro1 = strtok(NULL," ");
        parametro2 = strtok(NULL," ");


        if(strcmp(comando,"help")==0){
            if(parametro1 == NULL){
                printf("Lista de comandos implementados:\n\n");
                printf("init\nload\nls\nmkdir\nrmdir\ncreate\nrm\nwrite\ncat\nhelp\nman\n\n");
                printf("Para obter informacoes mais detalhadas dos comandos digite: man <comando>\n");
            }
            else{
                printf("O comando help nao possui parametros.\n");
            }
        }
        else if(strcmp(comando,"man")==0){
            if(parametro1 == NULL){
                printf("Numero de argumentos incorreto.\n\n");
                printf("Sintaxe: man <comando>\n");
            }
            else if(strcmp(parametro1,"init")==0){
                printf("O comando init cria um arquivo chamado fat.part com 4mb de tamanho.\n");
                printf("Estrutura do arquivo:\n");
                printf("================\n");
                printf("||    boot    ||\n");
                printf("||------------||\n");
                printf("||    FAT     ||\n");
                printf("||------------||\n");
                printf("||  root_dir  ||\n");
                printf("||------------||\n");
                printf("||   DADOS    ||\n");
                printf("================\n");
                printf("O arquivo esta dividido em blocos de dados, sendo que cada bloco possui o tamanho de 4kb.\n");
                printf("O primeiro bloco contém o boot do sistema\n.");
                printf("O segundo bloco contem a tabela FAT.\n\t Cada entrada na tabela pode conter 3 tipos diferentes de valores:\n");
                printf("\t0x00000000 --> cluster livre\n");
                printf("\t0x00000002 até 0xfffffffe--> posicao do proximo cluster a ser lido\n");
                printf("\t0xffffffff --> fim de arquivo\n");
                printf("O terceiro bloco contém o root_dir, o qual pode contem 128 entradas de diretorio(32 bytes cada).\n");
            }
            else if(strcmp(parametro1,"load")==0){
                printf("O comando load carrega o arquivo fat.part para a memoria caso este exista.\n");
            }
            else if(strcmp(parametro1,"ls")==0){
                printf("O comando ls lista todos os arquivos e diretórios contidos no diretório especificado.\n");
                printf("Parametros:\n");
                printf("-a --> lista tudo, incluindo arquivos ocultos.\n");
                printf("-d --> lista somente diretorios.\n");
                printf("-f --> lista somente arquivos.\n\n");
                printf("Sintaxe: ls <caminho/do/diretorio> -<parametro>\n");
            }
            else if(strcmp(parametro1,"mkdir")==0){
                printf("Cria um diretorio no path especificado, se este não existir.\n\n");
                printf("Sintaxe: mkdir path/<nome_diretorio>\n");
            }
            else if(strcmp(parametro1,"rmdir")==0){
                printf("Remove o diretorio no path especificado, se este existir.\n\n");
                printf("Sintaxe: rmdir path/<nome_diretorio>\n");
            }
            else if(strcmp(parametro1,"create")==0){
                printf("Cria um arquivo no path especificado, se este não existir.\n\n");
                printf("Sintaxe: create path/<nome_arquivo>\n");
            }
            else if(strcmp(parametro1,"rm")==0){
                printf("Remove o arquivo no path especificado, se este existir.\n\n");
                printf("Sintaxe: rm path/<nome_arquivo>\n");
            }
            else if(strcmp(parametro1,"write")==0){
                printf("Escreve a string digitada no arquivo escolhido, se este existir.\n\n");
                printf("Sintaxe: write \"string\" path/<nome_arquivo>\n");
            }
            else if(strcmp(parametro1,"cat")==0){
                printf("Lista o conteudo do arquivo escolhido, se este existir.\n\n");
                printf("Sintaxe: cat path/<nome_arquivo>\n");
            }else if(strcmp(comando,"exit")==0){
                return 0;
            }
        }

        else if(strcmp(comando,"init")==0){
            if(parametro1 != NULL){
                printf("O comando init nao possui parametros.\n");
            }
            init();
        }
        else if(strcmp(comando,"load")==0){
            load();
        }
        else if(strcmp(comando,"ls")==0){
            ls(parametro1,parametro2);
        }
        else if(strcmp(comando,"mkdir")==0){
            mkdir_create(parametro1,1);
            //varrer_disco_por_setor();
        }
        else if(strcmp(comando,"rmdir")==0){
            rmdir(parametro1);
        }
        else if(strcmp(comando,"create")==0){
            mkdir_create(parametro1,0);
        }
        else if(strcmp(comando,"rm")==0){
            rmdarnis(parametro1);
        }
        else if(strcmp(comando,"write")==0){
            write(parametro1,parametro2);
        }
        else if(strcmp(comando,"cat")==0){
            cat(parametro1);
        }
        else if(strcmp(comando,"varrer")==0){
            varrer_disco_por_setor();
        }
        else if(strcmp(comando,"exit")==0){
            fclose(ptr_myfile);
            return 0;
        }
    }
    return 0;
}

