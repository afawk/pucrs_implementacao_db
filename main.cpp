#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <stdint.h>
#include <string.h>
#include <iostream>
using namespace std;

//CONSTANTES
#define INICIO_AREA_DADOS 5
#define INICIO_AREA_INFO 2

//ESTRUTURAS DE DADOS

typedef struct {
    uint8_t isFull; // 1 byte
    uint8_t rowId;
    uint8_t block[4094];

} datablock;

typedef struct{
    datablock content[256];
    bool isOld[256];
} databuffer;


//OS PRIMEIROS 5 DATABLOCKS SAO RESERVADOS PARA USO DO SISTEMA
typedef struct{
    datablock file[65536];
} datafile;


//PROTOTIPOS DAS FUNÇÕES UTILIZADAS
bool init();
bool start();
bool write_json_file_to_datafile(char *filename);
bool write_json_string_to_datafile(char *text);
void go_to_beginning_of_data_area();
void go_to_datablock(unsigned short index);
void scan_datafile_for_analisys();
void scan_datablock_for_analisys(int index);
int randInRange();
char *readJson(uint16_t rowId);
char *readJsonBatch(uint16_t *rowIds);
int getNewRowId();
bool updateLastRowId(int rowId);
datablock getDataBlockFromBufferOrCacheIt(datablock block);
bool areDataBlocksEquals(datablock d1, datablock d2);

//VARIAVEIS GLOBAIS
/*
SEEK_SET * (4096*5)   INICIO DA AREA DE DADOS DO DATAFILE
SEEK_CUR    POSICAO ATUAL DO PONTEIRO DO ARQUIVO
SEEK_END    FINAL DO ARQUIVO *
 */

FILE *ptr_datafile;
uint8_t temp_block[4096];
databuffer buffer;

main() {
    //printf("Rodando");
    init();
    start();

    char input[5000];
    char inputParametro1[5000];
    char inputParametro2[5000];
    char *comando;
    char *parametro1;
    char *parametro2;

    while (1) {
        printf(" Mini Simulador de Sistema de Gestão de Metadados\n");
        printf(" * string - Escrever string json\n");
        printf(" * write - Escrever arquivo json\n");
        printf(" * get-id - Buscar através do id em documento json\n");
        printf(" * get-tag - Buscar através da tag e valor em documento json\n");
        printf(" * del - Exclui através do id em documento json\n");
        printf(" * readbatch - Insere usando multiplos arquivos json\n");
        printf("\n >> ");

        scanf(" %[^\n]s", input);
        comando = strtok(input, " ");

        if (strcmp(comando, "string") == 0) {
            printf(" String em formato json a ser gravado (entre aspas):\n");
            scanf(" %[^\n]s", inputParametro1);
            parametro1 = strtok(inputParametro1, " ");

            write_json_string_to_datafile(parametro1);
        }
        else if (strcmp(comando, "write") == 0) {
            printf(" Endereço do arquivo em formato json a ser gravado:\n");
            scanf(" %[^\n]s", inputParametro1);
            parametro1 = strtok(inputParametro1, " ");

            write_json_file_to_datafile(parametro1);
        }
        else if (strcmp(comando, "get-id") == 0) {
            printf(" ID do documento JSON:\n");
            scanf(" %[^\n]s", inputParametro1);
            parametro1 = strtok(inputParametro1, " ");

            //write_json_string_to_datafile(parametro1);
        }
        else if (strcmp(comando, "get-tag") == 0) {
            printf(" Tag e valor (separado por espaço) do documento JSON:\n");
            scanf(" %[^\n]s", inputParametro1);
            scanf(" %[^\n]s", inputParametro2);

            parametro1 = strtok(inputParametro1, " ");
            parametro2 = strtok(inputParametro2, " ");

            //write_json_string_to_datafile(parametro1);
        }
        else if (strcmp(comando, "del") == 0) {
            printf(" ID do documento JSON:\n");
            scanf(" %[^\n]s", inputParametro1);
            parametro1 = strtok(inputParametro1, " ");

            //write_json_string_to_datafile(parametro1);
        }
        else if (strcmp(comando, "readbatch") == 0) {
            printf(" Quantidade de documentos JSON:\n");
            scanf(" %[^\n]s", inputParametro1);
            parametro1 = strtok(inputParametro1, " ");

            //write_json_string_to_datafile(parametro1);
        }
        else if (strcmp(comando, "exit") == 0) {
            return 0;
        }

        printf("\n\n");
    }
    //scan_datafile_for_analisys();
    //scan_datablock_for_analisys(3);
    //getNewRowId();
    //write_json_string_to_datafile("[{color:\"red\",value:\"#f00\"}]");
}

/*Abre o arquivo do banco de dados para leitura */
bool start(){
    ptr_datafile = fopen("datafile.bd","w+b");
    if (!ptr_datafile)
    {
        printf("Não foi possivel abrir o datafile!");
        return 1;
    }

    int i;
    for(i=0;i<sizeof(temp_block);i++){
        temp_block[i] = 0x00000000;
    }

    return 0;
}

/*Cria o datafile de 256mb contendo 65536 datablocks com 4kb de tamanho cada.*/
bool init(){
    if (ptr_datafile)
    {
        fclose(ptr_datafile);
    }

    if(remove("datafile.bd") == 0){
        printf(">> Deletando datafile\n");
    }

    //printf(">> Inicializando datafile.\n");

    ptr_datafile = fopen("datafile.bd","w+b");

    //Se ocorreu algum problema ao tentar abrir o arquivo
    if (!ptr_datafile)
    {
        printf("Não foi possivel abrir o datafile!");
        return 1;
    }

    // cout << "Tamanho do datafile criado:" <<sizeof(datafile)/(1024*1024) << "Mb\n";
    // cout << "Numero de datablocks:" <<sizeof(datafile)/sizeof(datablock) << "\n";

    //Cria um datablock zerado para preencher todo os slots do datafile
    int i;
    for(i=0;i<sizeof(temp_block);i++){
        temp_block[i] = 0x00000000;
    }

    //Preenche todo os 65536 slots do datafile com 0x0
    for(i=0; i<sizeof(datafile)/sizeof(datablock); i++){
        fwrite(temp_block , 1 , sizeof(datablock) , ptr_datafile );
    }

    fflush(ptr_datafile);

    fclose(ptr_datafile);

    //Inicializa o buffer com vazio e todas as flags como falso
    databuffer buffer;

    for(i=0;i < sizeof(buffer.isOld);i++){
        buffer.isOld[i]=false;
    }

    return 0;
}

/*Recebe uma string JSON e grava em um datablock aleatorio*/
bool write_json_string_to_datafile(char *text){
    uint32_t text_size = strlen(text);

    int num_data_blocks= text_size/sizeof(datablock);

    cout << "String Json sendo gravada: " << text << endl;

    //Vai para um datablock aleatorio
    //Procura pelo primeiro datablock aleatorio que nao esteja cheio
    do {
        fread(&temp_block, sizeof(datablock),1,ptr_datafile);
        if(temp_block[0]==1){
            go_to_datablock(randInRange());
        }
    }while (1);

    datablock block;

    uint16_t newRowId;

    //Pega o rowId atual e adiciona 1
    block.rowId=getNewRowId();

    //Escreve o datablock de volta no banco
    fwrite(&temp_block, sizeof(datablock),1,ptr_datafile);

}

/*Procura por um datablock no buffer.
Se o datablock estiver no buffer, retorna-o. Do contrário, coloca-o no buffer(cache) utilizando o algoritmo CLOCK*/
datablock getDataBlockFromBufferOrCacheIt(datablock block){
    int i,j, clock;
    clock = 0;


    //Para cada datablock no buffer
    for (int i = 0; i < sizeof(buffer.content); i++) {
        datablock d = buffer.content[i];

        //Se encontrou o datablock no buffer, seta a flag para indicar que foi referenciado
        //e retorna o datablock
        if (areDataBlocksEquals(d,block)) {
            buffer.isOld[i] = true;
            return d;
        }
    }

    //Enquanto encontrar um datablock que foi referenciado(segunda chance), procura por um que nao tenha
    while(buffer.isOld[clock]) {
        buffer.isOld[clock] = false;
        clock += 1;
        if (clock >= sizeof(buffer.content))
            clock = 0;
    }

    buffer.content[clock] = block;
    if (clock >= sizeof(buffer.content))
            clock = 0;
    return block;

}

//Metodo auxiliar para comparar as structs pelo rowId
bool areDataBlocksEquals(datablock d1, datablock d2)
{
    if (d1.rowId == d2.rowId)
        return true;
    return false;
}

//Atualiza o ultimo numero de rowId gerado.
bool updateLastRowId(int rowId){
    fseek(ptr_datafile, 4096 * INICIO_AREA_INFO,SEEK_SET);
    fread(&rowId, sizeof(long),1,ptr_datafile);
    fwrite(&rowId, sizeof(long),1,ptr_datafile);
}

//Le um arquivo json e grava o conteudo no datafile
bool write_json_file_to_datafile(char *filename){
    int buffer_len = 4096;
    char content[buffer_len];

    memset(content, '\0', sizeof(content));

    FILE *f = fopen(filename, "r");

    if ( NULL == f ) {
        fputs("Erro ao abrir o arquivo", stderr);
        return 1;
    }

    memset(content, '\0', sizeof(content));

    //Passa para a variavel content o conteudo do arquivo
    int ret = fread(content, sizeof(char), buffer_len - 1, f);

    //Se ocorreu algum problema(ret <=0) gera erro e termina
    if (ret <= 0) {
        fputs("Problema ao ler o arquivo", stderr);
        return 1;
    }

    int content_len = sizeof(content);

    fclose(f);

    //Vai para o inicio da area de dados
    go_to_beginning_of_data_area();
}

//Coloca o ponteiro do datafile para o inicio do arquivo
void go_to_beginning_of_data_area(){
    fseek(ptr_datafile, 4096 * INICIO_AREA_DADOS,SEEK_SET);
}

//Vai para o datablock de numero index
void go_to_datablock(unsigned short index){
    fseek(ptr_datafile, 4096 * index,SEEK_SET);
}

//Funcao auxiliar parecida com hexdump para analisar o conteudo do datafile em hexa
void scan_datafile_for_analisys(){

    cout << "Criando log de analise do datafile" << endl;

    FILE *fp;
    const char* str = "DATABLOCK ";

    fp=fopen("datafile_hexa.txt", "w+b");
    if(fp == NULL)
        return;

    int counter;

    uint8_t temp_block[4096];
    int i;
    for(i=0;i<sizeof(temp_block);i++){
        temp_block[i] = 0x00000000;
    }


    fseek(ptr_datafile, 0,SEEK_SET); //fseek(FILE * stream, long int offset, int whence);
    for ( counter = 0; counter < 65536;counter ++) {
        //fseek(ptr_myfile, sizeof(cluster)*counter,SEEK_SET); //fseek(FILE * stream, long int offset, int whence);
        fread(&temp_block, sizeof(temp_block),1,ptr_datafile); //fread ( void * ptr, size_t size, size_t count, FILE * stream );

        fprintf(fp, "\n-----------%s %d--------------",str,counter+1);
        fprintf(fp, "Endereco de memoria inicial: %lu\n",sizeof(temp_block)*counter);

        int j;
        for (j = 0; j < sizeof(temp_block);j ++) {
            fprintf(fp, "%x ", temp_block[j]);
        }
    }
    fclose(fp);

    cout << "Log de analise do datafile criado";
}

/*void scan_datablock_for_analisys(int index){

    cout << "Criando log de analise do datablock" << endl;

    FILE *datafile_hexa;
    const char* str = "DATABLOCK ";

    datafile_hexa=fopen("datafile_hexa.txt", "w+b");
    if(datafile_hexa == NULL)
        return;

    int counter;

    uint8_t temp_block[4096];
    int i;
    for(i=0;i<sizeof(temp_block);i++){
        temp_block[i] = 0x0;
    }

    fseek(ptr_datafile, 4096 * index,SEEK_SET);
    fread(&temp_block, sizeof(temp_block),1,ptr_datafile);

    fwrite(datafile_hexa, "\n-----------%s %d--------------",str,counter+1);
    fwrite(datafile_hexa, "Endereco de memoria inicial: %lu\n",sizeof(temp_block)*index);

    int j;
    cout << sizeof(temp_block) ;
    for (j = 0; j < sizeof(temp_block);j ++) {
        //printf("%x ", cluster[j]);
        fprintf(datafile_hexa, "%x ", temp_block[j]);
        cout << sizeof(temp_block) << " " << endl;
    }

    fclose(datafile_hexa);

    cout << "Log de analise do datafile criado";
}*/

//Gera um numero aleatorio entre 5 e 65536(datablocks permitidos para gravar dados)
int randInRange(){
    //int min = INICIO_AREA_DADOS;
    //int max = sizeof(unsigned short);
    //return min + (int) (rand() / (double) (RAND_MAX + 1) * (max - min + 1));

    return rand() % sizeof(datafile)/sizeof(datablock) + INICIO_AREA_DADOS;

}

//Le arquivo JSON do datafile
char *readJson(uint16_t rowId){}

//Le um batch de arquivos JSON do datafile, recebendo como parametro um array de inteiros(rowIds)
char *readJsonBatch(uint16_t *rowIds){}

//Pega no datablock 2(area reservada) o ultimo rowId gerado e retorna o valor + 1
int getNewRowId(){

    long rowId;
    rowId = 0;

    fseek(ptr_datafile, 4096 * INICIO_AREA_INFO,SEEK_SET);
    fread(&rowId, sizeof(long),1,ptr_datafile);

    updateLastRowId(rowId+1);
    return rowId+1;
}