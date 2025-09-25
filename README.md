# PintOS - Guia de Execução

Este projeto utiliza o sistema operacional educacional **PintOS**. Siga os passos abaixo para rodar o projeto localmente.

## Pré-requisitos

- **Sistema Operacional:** Linux (recomendado Ubuntu)
- **Pacotes necessários:** `build-essential`, `qemu`, `gcc`, `binutils`, `make`, `gdb`

```bash
sudo apt update
sudo apt install build-essential qemu gcc binutils make gdb
```

## Clonando o repositório

```bash
git clone <URL-do-repositório>
cd Projeto_Implementacao_Sistemas_Operacionais
```

## Compilando o PintOS

Acesse o diretório do projeto e execute:

```bash
cd pintos/src
make clean
make
```

## Rodando o PintOS

Para executar um teste ou o kernel:

```bash
cd pintos/src/threads/build
pintos --qemu -- run alarm-multiple
```

Substitua `alarm-multiple` pelo teste desejado.

## Debugando com GDB

```bash
pintos-gdb kernel.o
```

## Referências

- [Documentação oficial do PintOS](https://web.stanford.edu/class/cs140/projects/pintos/pintos_1.html)
- [Guia de instalação do PintOS](https://github.com/klange/pintos)

---