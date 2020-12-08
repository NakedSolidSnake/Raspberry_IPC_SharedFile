![Shared](https://miro.medium.com/max/3202/1*eqPvgEZA5iH0AXOnXSbO7g.png)
# Introdução
Antes de prosseguirmos recomendo a leitura dos README's [lib hardware](https://github.com/NakedSolidSnake/Raspberry_lib_hardware), [fork_exec](https://github.com/NakedSolidSnake/Raspberry_fork_exec).
No artigo anterior foi apresentado os _system calls_, _fork_ e _exec_, dessa forma é possível chamar outra aplicação através dessa combinação, essa técnica vai ser usada em quase todos os próximos artigos. Nesse artigo será apresentado a técnica conhecida como _shared file_, vamos lá!

# _Shared File_

_Shared File_ é o mecanismo IPC mais básico, que consiste simplesmente abrir um arquivo e escrever nele, para o caso onde deseja-se inserir informação, e para consumir essa informação se realiza a leitura do mesmo. A figura demonstra esse procedimento.

<p align="center">
    <img src="images/sharedfile.png">
</p>

Na figura é possível observar a comunicação entre dois processos distintos, sendo um o Produtor(_Button_), e o outro o Consumidor(_LED_).
Para esse cenário o Produtor irá inserir no arquivo compartilhado a informação que o Consumidor deverá assumir, ou seja, o processo _Button_ irá alternar o estado de uma variável interna entre os estados 0 e 1, onde 0 representa o LED desligado, e o 1 representa o LED ligado. A cada vez que o botão for pressionado, o estado contido no arquivo será alternado.

O controle de acesso desse arquivo é necessário a utilização de uma estrutura chamada de _struct flock_ que funciona como uma espécie de trava para o arquivo compartilhado, e através da função _fcntl_, essa função é responsável por aplicar operações em um arquivo, para a utilização dessa função é necessário incluir os seguintes _includes_:

```c
#include <unistd.h>
#include <fcntl.h>

int fcntl(int fd, int cmd, ... /* arg */ );
```

para mais informações sobre essa estrutura execute no terminal o comando:

```bash
$ man 2 fcntl
```

# A aplicação
Para exemplificar esse _IPC_ vou utilizar três programas, sendo o *launch_processes* que tem a função de lançar os processos de *led_process* e *button_process*, sendo um o Consumidor(*led_process) e o outro o Produtor(*button_process*).

## *launch_processes*
Esse programa tem a finalidade de clonar e carregar os outros dois programas usando a combinação _fork_ e _exec_.

Para implementar o *launch_processes* incluimos os _headers_ necessários.
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
```

Após o _main_ criamos duas variáveis para armazenar o PID do *button_process* e do *led_process*, e mais duas variáveis para armazenar o resultado caso o _exec_ venha a falhar.
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

E termina o *launch_processes* com sucesso.
```c
return EXIT_SUCCESS;
```

## *button_process*
Esse programa tem a finalidade de alterar o conteúdo do arquivo entre o valores 0 e 1 conforme o botão conectado à Raspberry Pi for pressionado.

Para implementar o *button_process* incluimos os *headers* necessários.
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <button.h>
```

Definimos o nome do arquivo que vai armazenar o estado gerado pelo botão.
```c
#define _1ms 1000
#define FILENAME "/tmp/data.dat"
```

Criamos e preenchemos o descritor referente ao pino que está conectado o botão. Nessa configuração o botão está conectado no pino 7, configurado como entrada, como _pull up_ habilitado.
```c
static Button_t button7 = {
    .gpio.pin = 7,
    .gpio.eMode = eModeInput,
    .ePullMode = ePullModePullUp,
    .eIntEdge = eIntEdgeFalling,
    .cb = NULL};
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

Neste trecho inicializamos o botão com o descritor preenchido anteriormente.
```c
if (Button_init(&button7))
    return EXIT_FAILURE;
```

Aqui reside o _core_ da aplicação, neste fragmento, o programa fica em _polling_ aguardando que o botão seja pressionado, caso não, aguarda 1 ms para não consumir processamento. Se for pressionado realiza algumas validações para realmente efetivar a troca de estado e interrompe o laço *while*.
```c
while (1)
{
    if (!Button_read(&button7))
    {
        usleep(_1ms * 40);
        while (!Button_read(&button7))
            ;
        usleep(_1ms * 40);
        state ^= 0x01;
        break;
    }
    else
    {
        usleep(_1ms);
    }
}
```

Aqui realizamos a abertura do arquivo, se o mesmo não existir será criado, caso não seja possível criá-lo retorna para o _while_ principal aguardando a próxima ação do botão.
```c
if ((fd = open(FILENAME, O_RDWR | O_CREAT, 0666)) < 0)
    continue;
```

Com o arquivo criado, setamos as configurações de proteção, nesse caso corresponde à travar para escrita, sempre partindo do início do arquivo, e recebe o PID do processo. Aplicamos a configuração, caso falhe é aguardado 1ms para uma nova tentativa.
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


## *led_process*
Esse programa tem a finalidade de ler o conteúdo do arquivo e setar o estado do LED conectado à Raspberry Pi, conforme o conteúdo do arquivo. 

Para implementar o *led_process* incluimos os _headers_ necessários.
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <led.h>
```

Definimos o nome do arquivo que vai recuperado o estado gerado pelo botão.
```c
#define FILENAME "/tmp/data.dat"
```

Criamos e preenchemos o descritor referente ao pino que está conectado o LED. Nessa configuração o LED está conectado no pino 0, configurado como saída.
```c
static LED_t led =
    {
        .gpio.pin = 0,
        .gpio.eMode = eModeOutput
    };
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

Neste trecho inicializamos o LED com o descritor preenchido anteriormente.
```c
if (LED_init(&led))
    return EXIT_FAILURE;
```

Aqui realizamos a abertura do arquivo, em modo de leitura, caso não seja possível obter o descritor, retorna e aplicação se encerra.
```c
if ((fd = open(FILENAME, O_RDONLY)) < 0)
    return EXIT_FAILURE;
```

Aqui preenchemos a estrutura com os parâmetros de trava para escrita, onde o arquivo é tipo trancado para escrita, sempre partindo do início do arquivo, e recebe o PID do processo.
```c
lock.l_type = F_WRLCK;
lock.l_whence = SEEK_SET;
lock.l_start = 0;
lock.l_len = 0;
lock.l_pid = getpid();
```

Aqui tentando obter o que o arquivo possui como parâmetros de configuração.
```c
while (fcntl(fd, F_GETLK, &lock) < 0)
    usleep(_1ms);
```

Caso seja diferente de destravado fechamos o arquivo e retornamos para a próxima interação.
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

Convertemos o valor lido e verificamos se o valor atual é diferente do valor anterior, caso for diferente aplica o novo estado, se não, então não aplica a modificação.
```c
state_curr = atoi(buffer);
if(state_curr != state_old){
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

A listagem completa dos três programas pode ser encontrada na pasta [src](https://github.com/NakedSolidSnake/Raspberry_IPC_SharedFile/tree/main/src).

# Conclusão
Apesar de ser uma implementação simples, esse exemplo possui uma alta incidência de concorrência no acesso ao arquivo, tive que inserir alguns atrasos para evitar a  concorrência. O exemplo utilizado nesse artigo, foi criado de forma pura, para utilizar somente esse mecanismo, sem interferência de outro IPC. Esse problema pode ser resolvido em conjunto com outro IPC.
