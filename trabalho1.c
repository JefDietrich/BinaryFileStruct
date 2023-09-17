#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

char MARCACAO_DELETADO = '#';

typedef struct
{
    char codCliente[12];
    char codVeiculo[8];
    char nomeCliente[50];
    char nomeVeiculo[50];
    char dia[4];
} LOCACAO_VEICULO;

typedef struct
{
    char codCliente[12];
    char codVeiculo[8];
} REMOCAO_VEICULO;

void inserirHeader(FILE* arquivoResultado) {
    rewind(arquivoResultado);
    int header;
    if(fread(&header, sizeof(int), 1, arquivoResultado) == 0) {
        header = -1;
        fwrite(&header, sizeof(int), 1, arquivoResultado);
    }
}

/* Estrutura do registro deletado */
/*   100         #          304       [ registro ] */
/* tamanho end.      proximo deletado [ registro ] */
void inserirRegistro(LOCACAO_VEICULO veiculoAInserir, FILE* arquivoResultado) {
    char registro[130];
    sprintf(registro, "%s|%s|%s|%s|%s|", veiculoAInserir.codCliente,
            veiculoAInserir.codVeiculo,
            veiculoAInserir.nomeCliente,
            veiculoAInserir.nomeVeiculo,
            veiculoAInserir.dia);

    int tamRegistro = strlen(registro), header, posicaoRegistro, tamRegistroDeletado;
    bool espacoEncontrado = false;

    rewind(arquivoResultado);    
    fread(&header, sizeof(int), 1, arquivoResultado);

    if(header != -1) {        
        // Até achar um espaço ou o header for igual a -1
        while(!espacoEncontrado && header != -1) {            
            fseek(arquivoResultado, header, SEEK_SET);

            // Guarda posição do registro para inserir depois caso o tamanho seja ideal   
            posicaoRegistro = ftell(arquivoResultado);

            // Verifica se o tamanho do registro deletado consegue armazenar o registro novo
            fread(&tamRegistroDeletado, sizeof(int), 1, arquivoResultado);                                                    
            if (tamRegistroDeletado >= tamRegistro) {   
                espacoEncontrado = true;                            
            } 

            // Anda um para pular o # e lê o próximo registro deletado
            fseek(arquivoResultado, sizeof(char), SEEK_CUR);
            fread(&header, sizeof(int), 1, arquivoResultado);
        }     
    }  
    
    if(!espacoEncontrado) {
        // Se não encontrou um espaço, insere no fim do arquivo
        fseek(arquivoResultado, 0, SEEK_END);                 
        fwrite(&tamRegistro, sizeof(int), 1, arquivoResultado);    
        fwrite(registro, sizeof(char), tamRegistro, arquivoResultado);
    } else {
        fseek(arquivoResultado, posicaoRegistro + sizeof(int), SEEK_SET);        
        // fwrite(&tamRegistro, sizeof(int), 1, arquivoResultado);    
        fwrite(registro, sizeof(char), tamRegistro, arquivoResultado);

        // Atualiza o header com a posição próximo registro deletado
        rewind(arquivoResultado);
        fwrite(&header, sizeof(int), 1, arquivoResultado);
    } 
}

void removerRegistro(REMOCAO_VEICULO veiculo_remover , FILE* arquivoResultado) {
    int header, contadorDeCaninho, i, tamanhoRegistro, posicaoRegistro;    
    bool registroEncontrado = false;
    char letraLida, codigoLido[20];

    rewind(arquivoResultado);    
    
    // Guarda o header porque usaremos lá para frente
    fread(&header, sizeof(int), 1, arquivoResultado);

    // Montamos o código do registro que queremos deletar. Ex: 111111111AAA1389
    char codigoRegistroDeletar[20];
    strcpy(codigoRegistroDeletar, veiculo_remover.codCliente);
    strcat(codigoRegistroDeletar, veiculo_remover.codVeiculo);

    while(fread(&tamanhoRegistro, sizeof(int), 1, arquivoResultado) != 0) {  
        posicaoRegistro = ftell(arquivoResultado); // armazena posicao do registro     
        contadorDeCaninho = 0; // contador de | 
        i = 0; // contador de quantos caracteres foram lidos

        fread(&letraLida, sizeof(char), 1, arquivoResultado);

        if(letraLida == MARCACAO_DELETADO) {
            // Se a letra lida for # quer dizer que o registro já foi deletado
            fseek(arquivoResultado, sizeof(char) * (tamanhoRegistro - 1), SEEK_CUR);                         
            continue;
        }

        // O while irá pegar o código cliente e o código veiculo. Ex de arquivo: 111111111|AAA1389|.....
        // Vai lendo um caracter por caracter e irá criar a string '111111111AAA1389'
        // e quando ler dois '|' paramos porque já temos nosso código que será deletado
        while(contadorDeCaninho < 2) {                                            
            if(letraLida == '|') {
                contadorDeCaninho++;
            } else {
                codigoLido[i] = letraLida;
                i++;
            }           
            fread(&letraLida, sizeof(char), 1, arquivoResultado); 
        }
        codigoLido[i] = '\0';    
        
        // Comparamos se esse registro que queremos deletar
        if(strcmp(codigoLido, codigoRegistroDeletar) == 0) {
            registroEncontrado = true;
            break;
        } else {
            // Vai para o próximo registro: avança o tamanhoRegistro - (caracteres lidos + 2 caracteres '|')           
            fseek(arquivoResultado, sizeof(char) * (tamanhoRegistro - i - 3), SEEK_CUR);                      
        }            
    }

    if(registroEncontrado) {
        // Vai para o posição do começo do registro    
        //  aqui
        //   |
        //   10 111111 111|AAA1389|
        fseek(arquivoResultado, posicaoRegistro, SEEK_SET);        
        fwrite(&MARCACAO_DELETADO, sizeof(char), 1, arquivoResultado);
        fwrite(&header, sizeof(int), 1, arquivoResultado);

        // Vai para o começo do arquivo atualizar o header
        rewind(arquivoResultado);
        posicaoRegistro -= sizeof(int);
        fwrite(&posicaoRegistro, sizeof(int), 1, arquivoResultado);
    } else {
        printf("Registro não existe no arquivo\n");
    }
}

void compactarArquivo() {
    
    FILE* resultado = fopen("resultado.bin", "rb");
    FILE* temp = fopen("temp.bin", "w+b");

    char codigoLido[130];
    char letraLida;
    int i = 0, tamanho, contadorDeCaninho = 0;

    // Inseir -1 no header do arquivo temporário
    inserirHeader(temp);
    fread(&tamanho, sizeof(int), 1, resultado);
    
    while(fread(&tamanho, sizeof(int), 1, resultado) != 0) {
        contadorDeCaninho = 0; // contador de | 
        i = 0; // contador de quantos caracteres foram lidos

        fread(&letraLida, sizeof(char), 1, resultado);

        if(letraLida == MARCACAO_DELETADO){
            // Se a letra lida for # quer dizer que o registro já foi deletado então pulamos ele
            fseek(resultado, sizeof(char) * (tamanho - 1), SEEK_CUR);                         
            continue;
        }
        
        // Aqui recuperamos o registro e escrevemos no novo arquivo com o tamanho já atualizado (eliminamos a fragmentação interna)
        while(true) {                                            
            if(letraLida == '|') {
                contadorDeCaninho++;
            } 
            codigoLido[i] = letraLida;
            i++;                   

             if(contadorDeCaninho == 5) {
                break;
            }

            fread(&letraLida, sizeof(char), 1, resultado);            
        }
        codigoLido[i] = '\0';

        fwrite(&i, sizeof(int), 1, temp);
        fwrite(&codigoLido, sizeof(char), i, temp);   

        // Pula para o próximo registro
        fseek(resultado, sizeof(char) * (tamanho - i), SEEK_CUR);           
    } 

    fclose(temp);
    fclose(resultado);

    remove("resultado.bin");
    rename("temp.bin", "resultado.bin");    
}

FILE* verificaArquivo(char* arquivo) {
    FILE* fp = fopen(arquivo, "rb");
    
    if( (fp == NULL) ) {
        printf("O arquivo %s não existe.", arquivo);
        exit(0);
    }    
    return fp;
}

int main()
{
    FILE *arquivo = verificaArquivo("insere.bin");

    LOCACAO_VEICULO locacaoVeiculos[8];
    fread(locacaoVeiculos, sizeof(LOCACAO_VEICULO), 8, arquivo);

    arquivo = verificaArquivo("remove.bin");
    
    REMOCAO_VEICULO remocaoVeiculo[4];
    fread(remocaoVeiculo, sizeof(REMOCAO_VEICULO), 4, arquivo);

    fclose(arquivo);

    FILE *resultado = fopen("resultado.bin", "a+b");
    inserirHeader(resultado);
    fclose(resultado);

    int opcao, i = 0;

    do {
        printf("\n+------------------------------------------+");
        printf("\nSelecione uma das opções abaixo:\n\n");
        printf("(1) Inserir um registro.\n");
        printf("(2) Remover um registro.\n");
        printf("(3) Compactar o arquivo.\n");
        printf("(0) Sair do programa.\n\n");
        printf("\n+------------------------------------------+\n");
        printf("Opção: ");
        scanf("%d", &opcao);

        switch (opcao)
        {
            case 1:             
                while (1) {
                    printf("\nDigite '0' para sair.");      
                    printf("\nInforme um número de 1 a 8: ");      
                    scanf("%d", &i);

                    if (i == 0) {
                        break;
                    }else if (i < 1 || i > 8) {
                        printf("Opção inválida!");
                    }
                    resultado = fopen("resultado.bin", "r+b");
                    inserirRegistro(locacaoVeiculos[i-1], resultado);
                    fclose(resultado);
                }
                break;
            case 2:
                while (1) {
                    printf("\nDigite '0' para sair.");      
                    printf("\nInforme um número de 1 a 4: ");      
                    scanf("%d", &i);            

                    if (i == 0) {
                        break;
                    }else if (i < 1 || i > 4) {
                        printf("Opção inválida!");
                    }
                    resultado = fopen("resultado.bin", "r+b");
                    removerRegistro(remocaoVeiculo[i-1], resultado);
                    fclose(resultado);
                }
                break;        
            case 3:                
                compactarArquivo();
                printf("\nArquivo compactado com sucesso!\n");
                break;            
            case 0:
                printf("\nSaindo do programa...\n\n");
                break;            
            default:
                break;
        }
    } while (opcao != 0);    
}