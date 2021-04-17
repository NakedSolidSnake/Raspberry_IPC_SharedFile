![Shared](https://miro.medium.com/max/3202/1*eqPvgEZA5iH0AXOnXSbO7g.png)

Antes de seguir esse artigo é imprescindível a instalação da biblioteca [hardware](https://github.com/NakedSolidSnake/Raspberry_lib_hardware) caso queria utilizar o hardware da Raspberry.

# _Shared File_

## Introdução
_Shared File_ é o mecanismo IPC mais básico, que consiste simplesmente em manipular um arquivo comum com as operações básicas de _open_, _write_, _read_ e _close_, como essas operações é possível inserir informações no arquivo e bem como realizar a leitura dessas informações. Arquivos normalmente são usados para guardar diversos tipos de informações como configurações, logs, anotações entre outros, a figura baixo ajuda a ilustrar a comunicação entre dois processos por meio de uma arquivo.

<p align="center">
    <img src="images/sharedfile.png">
</p>

Na figura é possível observar a comunicação entre dois processos distintos, sendo um o Produtor(_Button_), e o outro o Consumidor(_LED_).
Para esse cenário o Produtor inserire no arquivo a informação que o Consumidor irá consumir, porém para que o acesso ao arquivo ocorra de forma sincronizada, faz-se necessário o uso da estrutura  _struct flock_ que funciona como uma espécie de chave para acessar o arquivo, que por meio da função _fcntl_, é possível verificar se o arquivo está com o acesso liberado, para poder manipulá-lo. A seguir podemos ver a _systemcall fcntl_:

```c
#include <unistd.h>
#include <fcntl.h>

int fcntl(int fd, int cmd, ... /* arg */ );
```

para mais informações sobre essa estrutura execute no terminal o comando:

```bash
$ man 2 fcntl
```

## Implementação

Para demonstrar o uso desse IPC, iremos utilizar o modelo Produtor/Consumir, onde o processo responsável por ser o Produtor(_button_process_) vai escrever seu estado interno no arquivo, e o Consumidor(_led_process_) vai ler o estado interno do botão e vai aplicar o estado para si. Aplicação é composta por três executáveis sendo eles:
* _launch_processes_ - é responsável por lançar os processos _button_process_ e _led_process_ atráves da combinação _fork_ e _exec_
* _button_interface_ - é reponsável por ler o GPIO em modo de leitura da Raspberry Pi e escrever o estado interno no arquivo
* _led_interface_ - é reponsável por ler do arquivo o estado interno do botão e aplicar em um GPIO configurado como saída

### *launch_processes*

No _main_ criamos duas variáveis para armazenar o PID do *button_process* e do *led_process*, e mais duas variáveis para armazenar o resultado caso o _exec_ venha a falhar.
```c
int pid_button, pid_led;
int button_status, led_status;
```

Em seguida criamos um processo clone, se processo clone for igual a 0, criamos um _array_ de *strings* com o nome do programa que será usado pelo _exec_, em caso o _exec_ retorne, o estado do retorno é capturado e será impresso no *stdout* e aborta a aplicação. Se o _exec_ for executado com sucesso o programa *button_process* será carregado. 
```c
pid_button = fork();

if(pid_button == 0)
{
    //start button process
    char *args[] = {"./button_process", NULL};
    button_status = execvp(args[0], args);
    printf("Error to start button process, status = %d\n", button_status);
    abort();
}   
```

O mesmo procedimento é repetido novamente, porém com a intenção de carregar o *led_process*.

```c
pid_led = fork();

if(pid_led == 0)
{
    //Start led process
    char *args[] = {"./led_process", NULL};
    led_status = execvp(args[0], args);
    printf("Error to start led process, status = %d\n", led_status);
    abort();
}
```

## *button_interface*

Definimos o nome do arquivo que vai armazenar o estado interno, e uma função de apoio para realizar a criação do arquivo que será usado para compartilhar as informações

```c
#define FILENAME "/tmp/data.dat"
static void create_file(void);
```

Aqui é criado uma variável para a formatação do dado que será armazenado no arquivo.
```c
char buffer[2];
```

Neste ponto é criada uma variável do tipo _flock_ que vai controlar o acesso ao arquivo. 
```c
struct flock lock;
```

Aqui é criado o descritor que é o responsável por armazenar o id do arquivo, e já aproveitamos para inicializar a variável _state_ que contém o estado que o LED vai assumir.
```c
int fd;
int state = 0;
```

Neste trecho inicializamos a interface do botão com o descritor preenchido conforme selecionado no momento da compilação.
```c
if (button->Init(object) == false)
    return EXIT_FAILURE;
```

Aqui reside o _core_ da aplicação, neste fragmento, o programa fica em _polling_ aguardando que o botão seja pressionado, caso não, aguarda 1 ms para não consumir processamento. Se for pressionado realiza a troca de estado e interrompe o laço *while*.
```c
while(true)
{
    if(!button->Read(object)){
        usleep(_1ms * 100);
        state ^= 0x01;
        break;
    }else{
        usleep( _1ms );
    }
}
```

Aqui é feita a abertura do arquivo
```c
if ((fd = open(FILENAME, O_RDWR, 0666)) < 0)
    continue;
```

Setamos as configurações de proteção, nesse caso corresponde em travar para escrita, sempre partindo do início do arquivo, e recebe o PID do processo. Aplicamos a configuração, caso falhe é aguardado 1ms para uma nova tentativa.
```c
lock.l_type = F_WRLCK;
lock.l_whence = SEEK_SET;
lock.l_start = 0;
lock.l_len = 0;
lock.l_pid = getpid();
while (fcntl(fd, F_SETLK, &lock) < 0)
    usleep(_1ms);
```

Com a trava obtida, iniciamos a formatação do dado para ser escrito no arquivo, e assim a sua escrita no arquivo.
```c
snprintf(buffer, sizeof(buffer), "%d", state);
write(fd, buffer, strlen(buffer));
```

Após a escrita configuramos para o arquivo para ser destravado para ser utilizado por outro processo.
```c
lock.l_type = F_UNLCK;
if (fcntl(fd, F_SETLK, &lock) < 0)
    return EXIT_FAILURE;
```

E por fim fechamos o arquivo e aguardamos 100ms e aguardamos a próxima interação com o botão.
```c
close(fd);
usleep(100 * _1ms);
```


## *led_interface*

Definimos o nome do arquivo que vai recuperado o estado gerado pelo botão.
```c
#define FILENAME "/tmp/data.dat"
```

Aqui é criado uma variável para a recuperação do dado que foi armazenado no arquivo.
```c
char buffer[2];
```

Neste ponto é criada uma variável do tipo _flock_ que vai controlar o acesso ao arquivo. 
```c
struct flock lock;
```

Aqui é criado o descritor que é o responsável por armazenar o id do arquivo, e já aproveitamos para inicializar a variável *state_old* que contém o estado que o LED vai assumir, e por último criamos uma variável *state_curr*, que vai receber o valor contido no arquivo.
```c
int fd = -1;
int state_old = 0;
int state_curr;
```

Neste trecho inicializamos a interface do LED com o descritor preenchido conforme selecionado no momento da compilação.
```c
if (led->Init(object) == false)
    return EXIT_FAILURE;
```

Aqui realizamos a abertura do arquivo, em modo de leitura, caso não seja possível obter o descritor, retorna e aplicação se encerra.
```c
if ((fd = open(FILENAME, O_RDONLY)) < 0)
    return EXIT_FAILURE;
```

Aqui preenchemos a estrutura com os parâmetros de trava para escrita, onde o arquivo é travado para escrita, sempre partindo do início do arquivo, e passamos o PID do processo.
```c
lock.l_type = F_WRLCK;
lock.l_whence = SEEK_SET;
lock.l_start = 0;
lock.l_len = 0;
lock.l_pid = getpid();
```

Aqui obtemos as configuração atuais do arquivo, para podermos verificar quais são suas configurações
```c
while (fcntl(fd, F_GETLK, &lock) < 0)
    usleep(_1ms);
```

Caso esteja travado, fechamos o descritor e retornamos para o início do processo
```c
if (lock.l_type != F_UNLCK){
    close(fd);
    continue;
}
```

Se estiver destravado setamos a trava para modo de leitura.
```c
lock.l_type = F_RDLCK;
while (fcntl(fd, F_SETLK, &lock) < 0)
    usleep(_1ms);
```

Lemos o conteúdo do arquivo.
```c
while (read(fd, &buffer, sizeof(buffer)) > 0);            
```

Convertemos o valor lido e verificamos se o valor atual é diferente do valor anterior, caso for diferente aplica o novo estado, se não, não aplica a modificação.
```c
state_curr = atoi(buffer);
if(state_curr != state_old)
{
    LED_set(&led, (eState_t)state_curr);
    state_old = state_curr;
}
```

Após a leitura configuramos para o arquivo para ser destravado para ser utilizado por outro processo.
```c
lock.l_type = F_UNLCK;
if (fcntl(fd, F_SETLK, &lock) < 0)
    return EXIT_FAILURE;
```

E por fim fechamos o arquivo e aguardamos 100ms e aguardamos a próxima leitura.
```c
close(fd);
usleep(100* _1ms);
```

## Compilando, Executando e Matando os processos
Para compilar e testar o projeto é necessário instalar a biblioteca de [hardware](https://github.com/NakedSolidSnake/Raspberry_lib_hardware) necessária para resolver as dependências de configuração de GPIO da Raspberry Pi.

## Compilando
Para faciliar a execução do exemplo, o exemplo proposto foi criado baseado em uma interface, onde é possível selecionar se usará o hardware da Raspberry Pi 3, ou se a interação com o exemplo vai ser através de input feito por FIFO e o output visualizado através de LOG.

### Clonando o projeto
Pra obter uma cópia do projeto execute os comandos a seguir:

```bash
$ git clone https://github.com/NakedSolidSnake/Raspberry_IPC_SharedFile
$ cd Raspberry_IPC_SharedFile
$ mkdir build && cd build
```

### Selecionando o modo
Para selecionar o modo devemos passar para o cmake uma variável de ambiente chamada de ARCH, e pode-se passar os seguintes valores, PC ou RASPBERRY, para o caso de PC o exemplo terá sua interface preenchida com os sources presentes na pasta src/platform/pc, que permite a interação com o exemplo através de FIFO e LOG, caso seja RASPBERRY usará os GPIO's descritos no [artigo](https://github.com/NakedSolidSnake/Raspberry_lib_hardware#testando-a-instala%C3%A7%C3%A3o-e-as-conex%C3%B5es-de-hardware).

#### Modo PC
```bash
$ cmake -DARCH=PC ..
$ make
```

#### Modo RASPBERRY
```bash
$ cmake -DARCH=RASPBERRY ..
$ make
```

## Executando
Para executar a aplicação execute o processo _*launch_processes*_ para lançar os processos *button_process* e *led_process* que foram determinados de acordo com o modo selecionado.

```bash
$ cd bin
$ ./launch_processes
```

Uma vez executado podemos verificar se os processos estão rodando atráves do comando 
```bash
$ ps -ef | grep _process
```

O output 
```bash
cssouza  16871  3449  0 07:15 pts/4    00:00:00 ./button_process
cssouza  16872  3449  0 07:15 pts/4    00:00:00 ./led_process
```
Aqui é possível notar que o button_process possui um argumento com o valor 4, e o led_process possui também um argumento com o valor 3, esses valores representam os descritores gerados pelo _pipe system call_, onde o 4 representa o descritor de escrita e o 3 representa o descritor de leitura.

## Interagindo com o exemplo
Dependendo do modo de compilação selecionado a interação com o exemplo acontece de forma diferente

### MODO PC
Para o modo PC, precisamos abrir um terminal e monitorar os LOG's
```bash
$ sudo tail -f /var/log/syslog | grep LED
```

Dessa forma o terminal irá apresentar somente os LOG's referente ao exemplo.

Para simular o botão, o processo em modo PC cria uma FIFO para permitir enviar comandos para a aplicação, dessa forma todas as vezes que for enviado o número 0 irá logar no terminal onde foi configurado para o monitoramento, segue o exemplo
```bash
$ echo "0" > /tmp/shared_file_fifo
```

Output do LOG quando enviado o comando algumas vezez
```bash
Apr 17 07:15:22 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: On
Apr 17 07:15:23 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: Off
Apr 17 07:15:24 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: On
Apr 17 07:15:24 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: Off
Apr 17 07:15:25 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: On
Apr 17 07:15:25 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: Off
Apr 17 07:15:26 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: On
Apr 17 07:15:26 cssouza-Latitude-5490 LED SHARED FILE[16872]: LED Status: Off
```

### MODO RASPBERRY
Para o modo RASPBERRY a cada vez que o botão for pressionado irá alternar o estado do LED.

## Matando os processos
Para matar os processos criados execute o script kill_process.sh
```bash
$ cd bin
$ ./kill_process.sh
```

## Conclusão
Apesar de ser uma implementação simples, esse exemplo possui uma alta incidência de concorrência no acesso ao arquivo, para funcionar de forma razoável foi necessário inserir alguns atrasos para evitar(reduzir) a  concorrência. Não é recomendado, pois precisa de outros mecanismos para sincronizar o seu acesso, como signal ou semaphore(que veremos mais adiante), mais a idéia é demonstrar o seu uso na forma pura sem interferência de outro IPC.

## Referência
* [Link do projeto completo](https://github.com/NakedSolidSnake/Raspberry_IPC_SharedFile)
* [Mark Mitchell, Jeffrey Oldham, and Alex Samuel - Advanced Linux Programming](https://www.amazon.com.br/Advanced-Linux-Programming-CodeSourcery-LLC/dp/0735710430)
* [fork, exec e daemon](https://github.com/NakedSolidSnake/Raspberry_fork_exec_daemon)
* [biblioteca hardware](https://github.com/NakedSolidSnake/Raspberry_lib_hardware)
