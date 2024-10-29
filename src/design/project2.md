# Project 2 - Design Report (Team 28)

## Project Description
Project 2는 크게 프로세스 종료 메세지 출력, 프로그램에 인자 전달, 시스템 호출, 
실행 파일에 대한 쓰기 거절의 네 가지 기능을 구현해야 한다.

### Process Termination Messages
일반적인 운영체제에서는 사용자 프로그램이 종료되면 그에 따른 종료 메세지를
프로세스의 반환값과 함께 출력한다. 이번 과제의 첫 번째 Task인 프로세스 종료
메세지 출력은 그와 같이, 현재 실행중인 프로세스가 종료되면 종료된 프로세스의
이름과 종료 코드(exit code)를 출력하도록 요구하고 있다.

### Argument Passing
프로그램을 실행할 때, 프로그램의 이름과 함께 명령행 인수(command line argument)
가 같이 전달되는 경우가 있다. 예를 들어, `cc -o hello hello.c`라는 명령어는
`cc`라는 프로그램을 실행하며 `cc`, `-o`, `hello`, `hello.c`라는 명령행 인수를
새로 만들어진, `cc`를 실행하는 프로세스에 전달한다. 이렇게 전달된 명령행 인수는
일반적인 C 프로그램의 경우 `main()`의 인수 `argv`에 전달되고 인수의 개수는 
`argc`에 전달되어, 프로그래머는 이러한 명령행 인수를 확인하여 프로그램의 동작을
적절하게 바꾸도록 프로그래밍하는 것이 일반적이다.

이러한 인수 전달 기능은 공짜로 제공되는 것이 아닌 운영체제의 추상화로 달성된다.
자세히 말하자면, 명령행 인수가 새로이 실행된 프로세스에 전달되기 위해서는 
명령행 인수를 whitespace 단위로 쪼개고(tokenize), 이들의 개수와 각 인수를 새로 
만들어진 프로세스의 스택에 넣어주어야 한다. Pintos 문서의 3.5.1 단락 Program 
Startup Details에서는 이러한 과정에 대해 자세히 서술하고 있다. 

### System Calls
많은 운영체제에서 사용자 프로그램은 기본적으로 사용자 모드에서 실행되며, 커널
권한이 필요한 명령어들은 사용자 모드에서 실행될 수 없다. 따라서, 만약 사용자
프로세스가 특정 커널 기능을 사용하고자 한다면 시스템 호출이라는 특별한 종류의 
예외(exception)를 발생시켜 커널에 해당 기능을 수행하도록 요청해야 한다. 즉,
시스템 호출은 사용자에게 공개된 커널에 대한 인터페이스 역할을 하여 사용자
프로세스가 커널 권한이 필요한 기능을 통제된 환경에서 수행할 수 있도록 한다.

시스템 호출은 사용자 프로그램에게는 하나의 함수 호출처럼 보여야 한다. 즉, 
사용자 프로세스는 시스템 호출과 함께 인자를 전달하고, 반환값이 있다면 반환값을
사용자 프로세스에서 사용할 수 있어야 하고, 시스템 호출이 끝나면 (시스템 호출이 
프로세스나 시스템을 종료시키지 않는다는 가정 하에) 시스템 호출의 다음 명령어에서 
프로세스 실행을 재개해야 한다. 이러한 추상화는 운영체제에 의해 제공된다. 즉, 
운영체제는 시스템 호출이 발생하면 인자를 시스템 호출 핸들러에 전달하고, 시스템 
호출 핸들러의 반환값을 다시 사용자 프로세스에 전달해야 한다.

이러한 시스템 호출에는 시스템의 실행을 중지하는 `halt()`, 프로세스의 실행을 자식
프로세스의 종료까지 잠시 멈추는 `wait()`, 실행 파일을 실행하는 `exec()` 등이 
있다. 

### Denying Write to Executables
만약 어떤 프로그램이 실행되는 도중에, 해당 실행 파일이 변경된다면 어떻게 될까? 
만약 실행 파일의 변경된 부분이 메모리에 이미 올라가 있고 이후 해당 페이지가 
메모리에서 다시 내보내지지(swap-out) 않는다면 프로그램은 변경되기 이전과 같이 
실행을 마칠 것이다. 만약 해당 변경된 부분이 메모리에 적재되어있지 않고, 새로
변경된 부분이 운좋게 현재 프로그램의 맥락과 일관적이라면 변경된 프로그램의
내용대로 실행될 수도 있을 것이다. 가장 최악의 경우, 실행 파일의 수정으로 인해
프로그램이 제대로 작동하지 않을 수도 있다. 즉, 프로그램 실행 도중 해당 파일의
변경은 예측할 수 없는 결과를 낳는다.

따라서, 많은 운영체제에서는 실행 파일이 실행되는 도중 해당 파일에 대한 쓰기를
제한한다. 예를 들어, Linux를 포함한 Unix 계열 운영체제에서는 파일이 실행되는 
도중 해당 파일에 쓰고자 하면 (일반적으로) ETXTBSY 오류를 발생시킨다. 이번 
프로젝트에서는 Pintos에서도 이러한 기능을 도입하여, 현재 실행중인 파일에 대한 
쓰기를 운영체제가 거부하도록 하는 것을 목표로 한다.

## Code Analysis
### Background of User Program 
이번 프로젝트에서는 이전 프로젝트와 달리 Kernel thread가 아닌, User Program을 실행시키기 위한 환경을 구현하는 것이 주요한 목표이다. 이를 위해 우선 pintOS에서는 각 User Program이 less priviliged mode에서 구동되며, 동시에 여러 process에서 실행될 수 있으며, 각 프로세느는 multi-thread를 허용하지 않는 one thread로 구성되어 있다는 것을 유념해야 한다. 따라서, 각 프로세스가 모든 machine을 사용할 수 있다는 가정하에 각 process의 schedule과 memeory manage 그리고 state 를 올바르게 관리하기는 것이 핵심이다. 

### Load User Program with File System
User Program 이 실행되기 위해서는 User Program을 담고 있는 file로 부터 읽어와서 실행해야 한다. 따라서 filesys에 구현되어 있는 코드를 통해 어떻게 Load 되는지 과정을 일부 이해해보고자 한다. 우선 Pintos 문서에 따르면 일부 제약사항이 있는 채로 구현이 되어있다고 한다. 제약사항은 다음과 같다. No internal Sync (자체적으로 Sync 를 위한 로직이 없기 때문에, Sync를 위해서 한 번에 한 Process 만 실행되도록 강제해야 한다), Fixed File Size (파일 사이즈가 고정되어 있으며 root directory 또한 고정되어 있어서 파일의 개수에 제한이 있다), Contigous Sector (한 데이터는 contiguous 하게 저장되어 있어야 한다), No Subdirectory, Limited File Names (14 글자 제한), No repair Tool(파일 시스템 실행 중 오류를 자동으로 복구할 방법이 없음), Filesys_remove (파일이 열려있는 동안 파일이 제거된다면, file descripter가 없는 processor에서는 더 이상 읽기/쓰기가 진행되지 않지만, file descripter가 이미 존재하고 있는 process에서는 process가 종료될 때까지 이미 제거된 파일이지만 지속적으로 읽기 쓰기가 가능하도록 구현되어 있다)

PintOS에서는 가상의 디스크를 생성하여 파일을 저장하고 이를 위에서 소개한 파일 시스템을 통해서 관리된다. 우선 가상의 디스크를 생성하고, 아래의 명령어들을 통해서 초기화와 실행을 확인해볼 수 있다.

```bash
pintos-mkdisk filesys.dsk --filesys-size=2
pintos -f -q
pintos -p ../../examples/echo -a echo -- -q
pintos -q run 'echo x'
```

각 명령어를 자세히 살펴보자면, pintos-mkdisk 프로그램을 통해서 filesys.dsk 라는 이름을 가진 2 MB의 용량을 가진 가상의 디스크를 생성하게 된다. 이후 pintos 를 통해서 -f (파일 시스템 포맷), -q (포맷 이후 바로 종료) 를 통해 디스크를 포맷하고, 이후 -p (put) 를 통해서 echo 프로그램을 파일 시스템 내로 복사하게 된다. 이 때 -a 를 통해 이름을 설정하며, -- -p 를 통해 시뮬레이션 된 커널에 -p 옵션이 전달되지 않도록 한다. 이때, 이 명령어들은 kernel의 command line에 실제로는 extract와 append를 통해서 전달되며, scratch partition으로부터 복사하여 전달되는 방식으로 구현되어 있으며 추후 코드 구현을 살펴보면서 더 자세히 살펴보고자 한다. 마지막 명령어는 echo 에게 x의 인자를 전달하는 방식으로 echo 를 실행시키게 된다. 

```bash
# two lines
pintos-mkdisk filesys.dsk --filesys-size=2
pintos -p ../../examples/echo -a echo -- -f -q run ’echo x’

# one line
pintos --filesys-size=2 -p ../../examples/echo -a echo -- -f -q run ’echo x’
```
이외에도 위의 구현과 같이 2줄로 구현을 할 수 있으며, 이외에도 file system disk를 남겨두고 싶지 않은 경우에는 1줄로도 실행할 수 있으며, 이 경우에는 n MB 사이즈를 가진 file system partition을 생성하게 되고, pintos 가 실행되는 중에만 파일이 존재하게 된다. 이외에도 file을 지우기 위한 rm (pintos -q rm file), list를 확인하기 귀한 ls와 파일의 내용을 확인하기 위한 cat을 사용할 수 있다. 이제 filesystem의 구현을 하나씩 살펴보자.

#### off_t.h
```C
/* An offset within a file.
   This is a separate header because multiple headers want this
   definition but not any others. */
typedef int32_t off_t;

/* Format specifier for printf(), e.g.:
   printf ("offset=%"PROTd"\n", offset); */
#define PROTd PRId32
```
offset을 지정할 때 사용되는 int32_t 자료형이 여러 파일에서 사용되므로 off_t.h 파일만을 통해서 off_t 를 지정해주고 있으며, offset을 prinf 를 통해서 출력될 때 사용되도록 stdint.h 에 저장되어 있는 PRId32를 활용하여 PROTd 매크로를 지정하여 printf에서 int32_t 가 올바르게 출력되도록 지정해주고 있다.

#### inode.c & inode.h
inode란 각 directory 가 갖는 meta data를 저장하기 위한 자료형으로 indoe.c 와 indoe.h 파일을 통해 구현되어 있다. 

```C
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
```
inode를 식별하기 위한 MAGIC 값이 선언되어 있다.

```C
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };
```
inode_disk는 디스크에 저장된 inode에 관련된 정보를 저장하고 있다. start를 통해 data sector의 시작과, length 를 통해 file의 size를 담고 있으며, magic을 통해 inode를 구별할 수 있게 된다. 또한, unused의 경우 BLOCK_SECTOR_SIZE (512) 의 크기를 맞춰주기 위해서 사용되지 않지만, 공간을 할당하게 된다. 이렇게 맞춰주는 이유는 Block (sector) 단위로 데이터가 읽고 쓰이게 되는데, 이 사이즈에 맞춰서 inode와 같은 구조체를 할당해야지 이후에 용량을 관리하기에 수월하기 때문이다.

```C
struct inode {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
};
```
inode 의 경우 파일을 관리하기 위한 메타 데이터로 이루어져 있으며, 리스트에 사용되기 위한 elem, 현재 Inode가 저장되어 있는 위치(섹터 번호)를 담고 있는 sector, 현재 해당 파일을 열고 있는 프로세스의 숫자인 open_cnt, 지워진 여부를 담고 있는 removed, 쓰기 권한을 관리하기 위한 deny_write_cnt, 마지막으로 디스크에 저장되어 있는 Indoe content (inode_disk)를 담고 있는 data로 구성되어 있다. 실제 파일이 저장되는 구조는, tmp.txt 라는 파일이 파일 시스템 내에 저장되게 된다면, inode의 번호로 매핑이 되고, inode 구조체는 메모리 상에서 tmp.txt 파일의 위치를 포함한 모든 정보를 저장하게 된다. 여기서 inode_disk는 disk 내에 inode를 저장하게 되는 것으로 이 구조체가 영구적으로 디스크에 저장되며 inode_disk로부터 데이터를 다시 읽어오는 방식으로 파일의 읽기와 쓰기가 작동된다.

```C
static inline size_t bytes_to_sectors (off_t size)
```
이 함수에서는 파일의 크기를 sector 개수로 올림하여 반환하게 된다. 


```C
static block_sector_t byte_to_sector (const struct inode *inode, off_t pos)
```
특정 byte의 위치가 어떤 sector에 위치하는지를 확인하는 함수로, data.length가 pos 보다 작게 된다면, 해당 byte 가 파일 내에 위치해 있으므로, 파일의 시작 + offset을 통해서 반환하게 되고, 파일 외부에 있다면 -1을 반환하게 된다.


```C
static struct list open_inodes;
```
현재 열려있는 Inode를 담고 있는 리스트로 리스트를 통해서 inode의 중복을 방지하고, 두 번 이상 열릴 경우 같은 struct inode를 반환하게 된다.

```C
void
inode_init (void) 
```
open_inodes 리스트를 초기화하는 부분이다.

```C
bool
inode_create (block_sector_t sector, off_t length)
```
매개변수로 입력받은 sector 위치에 inode_disk 를 생성하여 저장하는 함수로, 특정 length Bytes 길이를 가진 파일에 대응하는 inode를 생성하고 이를 메모리에 저장하여 반환하게 된다. 이 과정 중에 실패할 경우 false를 반환하게 된다.

```C
struct inode *
inode_open (block_sector_t sector)
```
sector로부터 inode를 읽어오는 연산을 수행하며, 우선 open_inodes를 순회하면서 열려있는지 확인하고, 해당 sector 내에 inode가 저장되어 있다면 struct inode를 새롭게 선언하여 반환하게 되고, 없다면 null pinoter를 반환하게 된다. 

```C
struct inode *
inode_reopen (struct inode *inode)
```
기존에 열려있는 Inode를 추가로 열 때 사용하는 함수로, Open_cnt 를 늘리게 된다.

```C
block_sector_t
inode_get_inumber (const struct inode *inode)
```
inode가 저장되어 있는 sector의 번호를 반환하게 된다.

```C
void
inode_close (struct inode *inode) 
```
열려있는 Inode를 닫는 함수로, open_cnt를 확인하여 마지막으로 혼자서 inode를 읽고 있다면 free 해주게 되고, 만약 removed가 true 로 되어있는 경우에도 마찬가지로 free 해주게 된다.

```C
void
inode_remove (struct inode *inode) 
```
직접 Inode를 삭제하는 것이 아닌 inode를 지우도록 removed를 true로 바꿔준다

```C
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
```
inode 위치에서 offset만큼 떨어진 위치부터 size 만큼의 데이터를 읽어서 Buffer에 저장하는 함수로 실제로 byte_to_sector를 통해 sector의 index로 변환하여 각 Index를 증가시켜가면서 inode를 읽어오게 된다. 이후 실제로 읽은 byte 수를 반환하게 되고, 이를 size와 비교하여 EOF에 도달했는지 혹은 오류가 발생했는지 여부를 함께 확인할 수 있다.

```C
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
```
위의 함수와 비슷하게 inode에서 offset만큼 떨어진 위치부터 시작하여 Buffer에 저장되어 있는 내용들을 inode에 저장하게 된다. 마찬가지로 size와 비교하는 과정을 통해 에러의 발생 유무와 EOF에 도달했는지 여부를 확인할 수 있으며, 일반적으로는 EOF 에 도달하게 되며 아직 파일의 사이즈를 키우는 연산은 구현되어 있지 않다. 또한, 구현된 함수 내에서 만약 chunk 가 남아있는 sector 의 크기보다 작은 경우에는 기존에 저장되어 있는 데이터가 존재할 수 있으므로 우선적으로 데이터를 읽고 이 이후에 write을 수행해야 한다. 만약 아니라면 해당 sector를 0으로 덮고 write을 진행하게 된다.

```C
void
inode_deny_write (struct inode *inode) 
```
inode에 대해 write을 금지하는 함수로, deny_write_cnt 를 증가시키게 된다.

```C
void
inode_allow_write (struct inode *inode) 
```
다시 write을 허용하는 함수인데, 위의 deny 함수를 실행시킨 opener 각각이 inode를 닫기 전에 단 한번씩만 수행해야 하며 deny_write_cnt 를 감소하도록 한다.

```C
off_t
inode_length (const struct inode *inode)
```
inode->data.length 를 반환하며 inode data의 length 를 반환한다.

#### file.c & file.h
이제 파일이 실제 어떻게 구성되어있는지 확인해보도록 하자. 위에서 inode를 메타데이터 관점에서 설명했지만, 사실 inode가 실제 어떤 위치에 어떻게 저장되어 있는지 등등을 전부 담고 있는 데이터라고 볼 수 있으며, struct file을 해당 inode를 어떻게 관리하는지 방식으로 구현되어 있다고 이해할 수 있다.

```C
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
```
struct file은 위에서 설명한 file의 정보를 담고 있는 inode, 현재 파일을 읽거나 쓰고 있는 위치를 담고 있는 pos, file의 쓰기를 허용하는지 여부를 담고 있는 deny_write으로 구성되어 있다.

```C
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}
```
실제 file_open 함수를 통해서 파일을 열게 되는데, 우선적으로 inode를 기반으로 파일을 구성하게 된다. 즉, 기존에 생성된 inode를 바탕으로 struct file을 생성하여 반환하게 되는 구조로 이뤄져 있으며, 만약 오류가 발생한 경우에는 NULL을 반환하게 된다.

```C
struct file *
file_reopen (struct file *file) 
{
  return file_open (inode_reopen (file->inode));
}
```
이미 기존에 열려있는 file을 다시 열 때 수행하는 함수로 inode_reopen을 통해서 inode cnt 개수를 늘려주고 이를 바탕으로 다시 file_open을 통해 새로운 file struct를 생성하게 된다. 아마 inode와 구현이 상이한 이유는 struct file의 경우 각각의 구조체가 현재 읽고 있는 pos 가 다를 수 있으므로 file 은 같은 파일을 열더라도 중복으로 선언하여 사용하지만 inode의 경우 파일의 메타데이터는 똑같으므로 하나의 구조체에서 cnt를 늘려가는 방식으로 구현되어 있는 것으로 생각된다.

```C
void
file_close (struct file *file) 
{
  if (file != NULL)
    {
      file_allow_write (file);
      inode_close (file->inode);
      free (file); 
    }
}
```
파일을 닫을 때 실행하는 함수로, 항상 allow_write를 통해 write을 허용한 뒤에 inode_close를 통해 inode 에도 현재 열려있는 개수를 조절하고 이후 free 해준다.

```C
struct inode *
file_get_inode (struct file *file) 
{
  return file->inode;
}
```
현재 파일에 해당하는 Inode를 반환한다.

```C
off_t
file_read (struct file *file, void *buffer, off_t size) 
{
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;
  return bytes_read;
}
```
file의 내용을 size Bytes 만큼 일어와서 buffer에 저장해주는 함수로 file 내의 pos 위치에서 시작으로 inode_read_at 함수를 통해 실제 읽어오는 연산을 수행하게 된다. 마찬가지로 size와 실제 반환된 bytes_read 크기의 비교를 통해 오류 발생 유무를 확인할 수 있다. inode_read_at 에서는 위에서 설명한 것처럼 inode의 위치 정보를 바탕으로 sector를 찾아 Block_read 를 통해 각 섹터에서 file 의 내용을 읽어오게 되는 방식으로 구현되어 있으며 읽어온 데이터는 Buffer에 저장된다. 파일을 읽은 만큼 pos이 이동하게 된다.

```C
off_t
file_write (struct file *file, const void *buffer, off_t size) 
{
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;
  return bytes_written;
}
```
파일에 쓰는 함수의 경우에도 Inode_write_at을 통해서 구현되어 있으며 inode가 담고 있는 위치 정보를 바탕으로 buffer에 있는 데이터를 inode + offset의 위치에 해당되는 sector 부터 쓰게 되는 방식으로 구현되어 있으며, 즉 file pos 위치에서 시작하여 size Bytes 만큼 쓰게 된다. 마찬가지로 return 되는 값을 size와 비교하여 오류 유무를 찾을 수 있으며, file의 크기를 키우는 growth 는 구현되어 있지 않은 상황이다. read와 마찬가지로 읽은 만큼 pos 이 이동하게 된다.

```C
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) 
{
  return inode_read_at (file->inode, buffer, size, file_ofs);
}
```
file_read와 매우 흡사하지만, pos 이 연산 전후로 바뀌지 않는다. 

```C
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs) 
{
  return inode_write_at (file->inode, buffer, size, file_ofs);
}
```
write와 동일하지만 pos 이 연산 전후로 바뀌지 않는다.

```C
void
file_deny_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (!file->deny_write) 
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
}
```
file이 닫히거나 file_allow_write() 이 실행되기 전까지는 write 연산을 수행하지 못하도록 막아주는 함수로 deny_write을 true로 설정하고 INode_deny_write을 실행하여 Inode_deny_cnt 도 늘려주게 된다.

```C
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (file->deny_write) 
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}
```
반대로 write을 허용해주는 함수이지만, 실제로 보면 write 시에 inode_write 통해서 구현이 되어있는데 만약 같은 inode를 가지고 있지만 여러 file을 통해 열려있는 상황이라면 해당 파일에서 allow_write을 통해 write을 허용해주더라도 다른 file에서 deny 를 했다면 file이 열리지 않을 수 있다. 즉, inode의 deny_write_cnt이 0이 될 때 까지는 쓰기가 허용되지 않을 수 있다.

```C
off_t
file_length (struct file *file) 
{
  ASSERT (file != NULL);
  return inode_length (file->inode);
}
```
inode_length를 통해서 파일이 차지하고 있는 크기를 반환한다.

```C
void
file_seek (struct file *file, off_t new_pos)
{
  ASSERT (file != NULL);
  ASSERT (new_pos >= 0);
  file->pos = new_pos;
}
```
file의 현재 읽기/쓰기 위치를 new_pos 로 새롭게 조절하는 역할을 담당하고 있다.

```C
off_t
file_tell (struct file *file) 
{
  ASSERT (file != NULL);
  return file->pos;
}
```
현재 file의 어느 부분을 읽기/쓰기가 진행되고 있는지(pos)를 반환하게 된다.

#### directory.c & directory.h
이제 디렉토리가 어떻게 관리되고 있는지를 알아보려 한다. directory 또한 file과 유사하게 inode를 기반으로 구현되어 있다고 이해할 수 있다. 즉, 디렉토리도 일종의 특수한 파일의 개념에서 바라볼 수 있으며 대신에 파일이 저장되어 있는 inode sector의 번호(혹은 서브디렉토리)와 파일의 이름을 담고 있는 dir_entry 로 구성되어 있는 특수한 파일이라 생각해볼 수 있다.

```C
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };
```
directory 자체로 현재 dir가 위치한 inode를 담고 있는 inode와 해당 디렉토리를 순회하기 위한 pos로 구성되어 있다.

```C
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };
```
각 디렉토리의 entry 로 파일의 이름과 해당 파일이 저장되어 있는 sector number를 담고 있는 Inode_sector, 현재 사용도고 있는지 여부를 담고 있는 in_use로 구성되어 있다.

```C
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}
```
dir_entry 개수만큼 dir_entry 가 들어갈 수 있도록 sector에 공간을 할당하는 방식으로 구현하는 함수로 마찬가지로 Inode_create을 통해서 생성하게 된다.

```C
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}
```
inode를 기반으로 해당 inode가 담고 있는 directory의 주소를 반환하여 여는 함수로 calloc을 통해서 새로운 dir 를 생성해주고 이후에 inode를 할당하고, pos를 0으로 설정하는 방식으로 dir 를 여는 것이 구현되어 있으며, 이렇게 생성된 struct dir 를 반환하게 된다.

```C
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Sectors of system file inodes. filesys.h */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */
```
root directory를 열고 그에 해당하는 struct dir 를 반환하는 함수로 filesys.h 에 지정되어 있는 root diectory의 sector를 토대로 inode를 찾아서 반환하고 그 inode를 통해서 root directory를 열게 된다.

```C
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}
```
이미 열려있는 directory의 inode와 같은 inode를 참고하는 directory를 열 때 사용하는 함수로 inode_reopen을 통해서 매개변수로 입력받은 directory의 inode를 다시 열고, 그 inode를 기반으로 directory를 다시 열게 된다. struct dir도 file과 마찬가지로 같은 inode를 읽더라도 중복으로 생성될 수 있다.

```C
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}
```
열려있는 dir를 닫는 함수로 현재 참고하고 있는 inode를 close 해주고, 이후 free를 통해서 directory를 닫게 된다.

```C
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}
```
현재 directory의 inode를 반환하게 된다.

```C
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}
```
매개변수로 입력받은 현재 dir 내에서 각 dir_entry를 순회하면서 name과 같은 파일이 존재하는지 여부를 반환하는 함수로, 만약 같은 이름을 가진 파일이 dir 내에 존재한다면 해당 dir_entry를 ep에 저장하고, 해당 dir_entry의 Byte offset을 ofsp에 저장하고 ture를 반환하며, 찾지 못한 경우 ep와 ofsp를 모두 무시하면서 false를 반환하게 된다. 

```C
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}
```
lookup 함수를 바탕으로 이 함수에서는 같은 기능을 수행하지만 만약 해당 name을 가진 file이 존재할 경우 ture를 반환하게 되고, inode 포인터에 해당 sector의 inode를 open 하여 반환하게 되지만, 찾지 못하게 된 경우에는 null pointer로 저장하며 false를 반환하게 된다.

```C
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  if (lookup (dir, name, NULL, NULL))
    goto done;

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}
```
이 함수에서는 말 그대로 name을 가진 file을 해당 dir에 추가하는 역할을 수행하는 함수로, 현재 추가하고자 파일의 inode의 sector number를 담고 있는 inode_sector를 입력받는다. 우선 Look 함수를 통해서 같은 name을 가진 file이 존재하는지 여부를 확인하게 되고, 없는 경우에만 추가하게 된다. 만약에 없는 경우에는 inode_read_at을 통해서 해당 Directory의 비어있는 slot을 차례로 찾아 나가게 되는데, 비어있는 슬롯이 없다면 맨 끝에 새로운 슬롯을 만들어 추가하게 된다. for 문에서 EOF 가 도달하거나 끝에 도달했는지 확인하는 방식은 inode_read_at에서 반환하는 Size 값이 어떤지에 따라서 반환하는 값이 달라지게 되는데, 끝에 도달할 경우 size보다 작은 크기를 반환하게 되므로 이를 통해서 비어있는 슬롯이 없는 것을 알 수 있게 된다. 실제 구현 상으로는 for 문의 종료 시 Ofs 가 가리키는 위치가 제일 끝이 될 것이며, for 문을 통해 비어있는 슬롯을 찾는 경우에 해당 자리에 inode_write_at을 통해 e의 내용을 덮어쓰게 되고 성공 여부를 반환하게 된다.

```C
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (!lookup (dir, name, &e, &ofs))
    goto done;

  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}
```
해당 dir 내에 name이라는 이름을 가진 entry를 지우는 연산을 수행하는 함수로 Lookup을 통해 해당 entry 를 찾고, inode를 open하여 열어준 다음, 이 Inode를 기반으로 inode_write_at을 통해서 해당 부분을 덮어쓰고, 이후에 inode를 지우게 된다. 중간 중간에 inode가 NULL 이거나 찾지 못한 경우에는 inode를 닫아주는 연산만 수행하고 성공 여부를 반환하게 된다.

```C
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}
```
현재 dir 내의 다음 entry를 읽어오는 연산으로 매개변수로 입력받은 dir를 통해서 다음 entry를 찾아나가는 방식으로 구현되어 있으며 pos를 하나씩 늘리면서 다음 entry 로 넘어가게 되고, 도달하는 entry의 name을 저장하여 반환하게 된다.

#### bitmap.c & bitmap.h
free-map 을 알아보기 전, free-map 이 구성되어 있는 bitmap 이라는 구조체에 대해서 먼저 잠시 필요한 연산 위주로 알아보고자 한다.

```C
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
  };
typedef unsigned long elem_type;
```
bitmap 은 말 그대로 여러 bit를 저장하고 있는 struct 로 bit_cnt는 비트의 개수를 저장하고 있으며, bits 는 unsigned long 으로 지정되어 있어서 0 번째 bit 부터 시작하여 bit_cnt 만큼 각 자리가 0 혹은 1을 저장하게 된다.

```C
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}
```
start 위치에서 시작하여 Cnt 개수 만큼의 Bit 을 value 로 설정하는 함수이다. bitmap_set은 간략하게 value 가 1이라면 bitmap_mark(특정 Index의 값을 1로 설정하는 함수), 0이라면 bitmap_set(특정 index의 값을 0으로 설정하는 함수)로 이뤄져 있다. 또한, 각 mark와 set 함수는 uniprocessor 에서 atomic 하게 작동하도록 assembly 연산으로 구현되어 있음을 알아두어야 한다. 따라서 bitmap의 값이 이상 없이 잘 관리되도록 한다.

```C
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}
```
위에서 설명한 set_multiple 함수를 통해서 전체 bitmap을 value 로 설정하는 함수이다.

```C
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}
```
bit_cnt 개수만큼의 Bit를 할당할 수 있는 bitmap을 생성하여 반환하는 함수이며, bitmap의 사용을 마친 이후에는 꼭 bitmap_destory를 통해서 할당된 공간을 해제해야 한다.

```C
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}
```
bitmap b 에서 start에서부터 찾기 시작하여 cnt 개수만큼의 bit가 value 랑 동일할 경우 그 부분 전체를 !value 로 설정하는 함수이다.

```C
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
```
bitmap b 의 내용을 file 에 저장하는 함수이다.

```C
bool
bitmap_read (struct bitmap *b, struct file *file) 
{
  bool success = true;
  if (b->bit_cnt > 0) 
    {
      off_t size = byte_cnt (b->bit_cnt);
      success = file_read_at (file, b->bits, size, 0) == size;
      b->bits[elem_cnt (b->bit_cnt) - 1] &= last_mask (b);
    }
  return success;
}
```

```C
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}
```
bitmap 의 크기를 반환하게 되는데, bit_cnt 를 기반으로 사이즈를 계산하여 반환하게 된다.

#### free-map.c & free-map.c 
위에서 소개한 bitmap을 기반으로 구현된 디스크 블록의 할당과 해제를 관리하게 되는 free map을 알아보고자 한다. Free map을 통해서 각 sector의 할당 여부를 알 수 있게 된다.

```C
static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */
```
free_map_file은 free_map 자체를 file 로 두고 관리하도록 하는 struct file이며 free_map 이 file 로 관리되어 디스크에 저장되게 된다. free_map 은 Bitmap 으로 선언되어 있어서 사용 여부에 따라서 0 혹은 1로 관리된다.

```C
void free_map_init (void)
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
}
```
free_map 을 초기화하는 함수로 위에서 선언한 free_map 에 file system device 가 가진 Block 의 숫자, 즉 전체 file system 의 sector 숫자 만큼을 할당하고 필수적으로 사용되는 free_map 을 저장하기 위한 sector 0 번과, root directory 를 저장하기 위한 섹터 1을 할당하게 marking 하게 된다.

```C
bool free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      bitmap_set_multiple (free_map, sector, cnt, false); 
      sector = BITMAP_ERROR;
    }
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
  return sector != BITMAP_ERROR;
}
```
현재 bitmap 에서 cnt 개수만큼 연속되면서 비어있는 sector를 찾아주는 함수로 해당 cnt 개수만큼 비어있는 sector를 찾는다면 그 부분 전체를 true 로 설정하게 되고, 이후에 sectorp 에 연속된 sector의 시작 번호를 반환하게 된다. 만약 오류가 발생한 경우에는 true로 매핑된 부분들을 다시 false로 설정하고 false를 반환하게 된다.

```C
void free_map_release (block_sector_t sector, size_t cnt)
{
  ASSERT (bitmap_all (free_map, sector, cnt));
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
}
```
연산은 bitmap_set_multiple 을 통해 현재 free_map 에서 sector 번호를 시작으로 Cnt 개수만큼을 false 로 설정하고, 이후에 그 free_map을 disk에 기록하게 되는 과정이다.

```C
void free_map_open (void)
{
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
}
```
이미 FREE_MAP_SECTOR(0) 에 저장되어 있는 free_map 을 읽어와서 free_map_file 에 올려주게 된다. 이후에 free_map_file 을 통해서 free_map 을 읽어오게 된다.

```C
void free_map_create (void)
{
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map)))
    PANIC ("free map creation failed");

  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
}
```
새로운 free_map 을 생성하는 함수로 free_map 을 생성하기 위한 inode를 우선적으로 선언하고 해당 Inode를 토대로 bitmap(free_map) 을 free_map_file 에 작성하게 된다.

```C
void free_map_close (void)
{
  file_close (free_map_file);
}
```
file system 종료 시 호출되는 함수로 현재 열려있는 free_map_file을 닫게 된다.

#### block.s & block.c (devices/block)
전반적인 file system을 알아보기에 앞서서 pintos 에서 데이터를 관리할 때 사용되는 Block system에 대해서 먼저 알아봐야 한다. 파일 시스템이나 swap, Kernel 등을 관리할 때 사용되는 struct로 이제 알아보고자 한다.

```C
#define BLOCK_SECTOR_SIZE 512
typedef uint32_t block_sector_t;
```
우선 각 block의 sector의 크기는 512 bytes로 지정되어 있으며, sector의 index를 나타내는 데이터 타입인 block_sector_t 는 unsigned int 32 로 이뤄져 있다. 이 type 으로는 약 2 TB 정도의 데이터까지를 할당할 수 있는 index를 저장할 수 있다.

```C
enum block_type
  {
    BLOCK_KERNEL,                /* Pintos OS kernel. */
    BLOCK_FILESYS,               /* File system. */
    BLOCK_SCRATCH,               /* Scratch. */
    BLOCK_SWAP,                  /* Swap. */
    BLOCK_ROLE_CNT,
    BLOCK_RAW = BLOCK_ROLE_CNT,  /* "Raw" device with unidentified contents. */
    BLOCK_FOREIGN,               /* Owned by non-Pintos operating system. */
    BLOCK_CNT                    /* Number of Pintos block types. */
  };
```
block이 가질 수 있는 type들로 kenel, filesystem, scratch, swap, raw, foreign, cnt 로 나눠져 있다. cnt는 개수를 의미하고 있으며, kernel와 filssys를 제외하고 Scracth 는 임시 저장용 block 이며, raw는 확실하게 지정되지 않은 내용으로 아마 Pintos에서 사용되지 않는 것으로 확인되고 있다. foreign 의 경우에는 pintos가 아닌 다른 OS가 관리하는 block이다.

```C
struct block
  {
    struct list_elem list_elem;         /* Element in all_blocks. */

    char name[16];                      /* Block device name. */
    enum block_type type;                /* Type of block device. */
    block_sector_t size;                 /* Size in sectors. */

    const struct block_operations *ops;  /* Driver operations. */
    void *aux;                          /* Extra data owned by driver. */

    unsigned long long read_cnt;        /* Number of sectors read. */
    unsigned long long write_cnt;       /* Number of sectors written. */
  };
```
struct block은 list를 위한 list_elem, 이름을 담고 있는 name, block 의 type을 저장하는 type, block이 몇개의 sector 로 저장되어 있는지 크기를 담고 있는 size, block 의 연산자인 ops, 그리고 추가 매개변수로 활용될 수 있는 aux, 현재 읽고 있는 섹터의 개수인 read_cnt, 현재 쓰고 있는 sector의 개수인 write_cnt 로 나눠져 있다.

```C
static struct list all_blocks = LIST_INITIALIZER (all_blocks);
static struct block *block_by_role[BLOCK_ROLE_CNT];
```
모든 block devices를 list로 만들어 all_blocks에 저장하게 되며, 각 역할에 따른 block을 담기 위해서 block_role 의 개수만큼의 block 을 선언하게 된다.

```C
struct block * block_get_role (enum block_type role)
void block_set_role (enum block_type role, struct block *block)
struct block *block_get_by_name (const char *name);
```
위에서 부터 각각 role 에 따라서 block 을 찾거나, 특정 block의 role 을 지정하거나, 특정 이름을 가진 block을 반환하게 되는 함수이다.

```C
static struct block * list_elem_to_block (struct list_elem *list_elem)
struct block * block_first (void)
struct block * block_next (struct block *block)
```
all_blocks 를 순회하기 위한 함수로 위에서부터 각각 list_elem 을 입력받아서 block 형태로 반환하는 함수와, all_block 리스트의 첫 번째 원소를 반환하거나, 현재 block 다음 block을 반환하는 함수이다.

```C
static void check_sector (struct block *block, block_sector_t sector)
void block_read (struct block *block, block_sector_t sector, void *buffer)
void block_write (struct block *block, block_sector_t sector, const void *buffer)
```
위에서부터 sector 가 valid 한 offset을 지니고 있는지 확인하게 된다. 즉, Block 내에서 매개변수로 입력받은 sector 가 valid 한지 확인하게 되고, 아니라면 panic 하게 된다. 두 번째 함수는 block 에서 특정 sector index 부분을 읽어서 Buffer에 저장하는 함수로 주석에는 함수 내에서 자동으로 동기화가 이뤄지기 때문에 외부에서 Per-block device의 Lock이 필요하지 않다고 명시되어 있으며, read 를 진행하고 read_cnt 를 늘리게 된다. block_write 도 비슷하게 buffer에 있는 내용을 Block의 특정 Sector에 작성하게 되며, buffer의 내용은 무조건 한 sector의 크기와 동일해야 한다. 마찬가지로 Lock 이 따로 필요하지 않다.

```C
block_sector_t block_size (struct block *block)
const char * block_name (struct block *block)
enum block_type block_type (struct block *block)
void block_print_stats (void)
```
위에서부터 각각 Block의 size, name, type, stats 를 출력 혹은 반환하는 함수이다.

```C
struct block *
block_register (const char *name, enum block_type type,
                const char *extra_info, block_sector_t size,
                const struct block_operations *ops, void *aux)
{
  struct block *block = malloc (sizeof *block);
  if (block == NULL)
    PANIC ("Failed to allocate memory for block device descriptor");

  list_push_back (&all_blocks, &block->list_elem);
  strlcpy (block->name, name, sizeof block->name);
  block->type = type;
  block->size = size;
  block->ops = ops;
  block->aux = aux;
  block->read_cnt = 0;
  block->write_cnt = 0;

  printf ("%s: %'"PRDSNu" sectors (", block->name, block->size);
  print_human_readable_size ((uint64_t) block->size * BLOCK_SECTOR_SIZE);
  printf (")");
  if (extra_info != NULL)
    printf (", %s", extra_info);
  printf ("\n");

  return block;
}
```
새로운 블록을 등록 사실상 생성하는 함수로 name, type 을 입력받아 설정하고, size 만큼의 sector 개수를 갖게 된다. 또한, 다양한 block type 이 존재하는 만큼 write, read 연산들은 ops 를 통해 입력받고 매개변수 또한 Aux 통해서 입력받도록 block device 을 등록하게 된다.

#### filesys.c & filesys.h 
이제 inode 부터 시작해서, inode를 통해 구현된 file과 directory를 알아보았으며, 부가적으로 필요한 block system과 bitmap 및 free_map 까지 알아보았으며, 그것들을 기반으로 대체 pintos에서는 어떻게 file system이 구현되어 있어서 초반 pintos-filesys 를 통해 생성한 그 시스템을 운용하는지 알아보고자 한다.

```C
extern struct block *fs_device;
```
우선 filesys.h 에서는 fs_device 를 선언하게 되는데, file system을 위한 struct block 을 선언하고 이를 기반으로 file system 을 구현하게 된다.

```C
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}
```
filesys_init 을 통해서 위에서 기존에 등록된 block 중에서 Filesys 라는 역할을 가진 Block 을 fs_device로 지정하게 된다. 이후 해당 block 을 가르키기 위한 inode를 init 해주고, 마찬가지로 Sector를 관리하기 위한 free_map 도 init 해주게 된다. 여기서 format 의 경우 pintos -f -q 라는 명령어를 입력하게 될 경우 -f option 이 포함된다면 실행되게 된다.

```C
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
```
말 그대로 현재 block device의 모든 sector를 초기화하는 함수로 전체 sector를 지우는 방식인데, 사실 free_map_create를 현재 Sector의 사용 여부에 관한 데이터를 초기화하는 방식으로 구현되어 있으며, 대신에 root_directory 와 Free_map directory 만 다시 세팅하여 아예 remove 하는 방식이 아닌 보다 간략하게 구현되어 있다.

```C
void
filesys_done (void) 
{
  free_map_close ();
}
```
file system을 닫을 때 사용되는 함수로 free_mep_close를 통해 현재 디스크에 저장되어 있지 않고, 메모리에만 저장되어 있는 데이터를 디스크에 저장하게 된다.

```C
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}
```
name 이라는 이름을 갖고 Inital_size 만큼의 크기를 갖는 file을 생성하여 현재 block device에 추가하는 함수로, free_map 에서 1개의 연속된 공간, 즉 그냥 추가할 수 있는 공간이 있는지 확인하고 새로운 파일을 가리키는 Inode를 선언하며 이를 directory 에 add 하여 할당하게 된다. 만약 실패한 경우 다시 free_map 을 release하게 된다.

```C
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}
```
현재 file system 내의 특정 Name 을 가진 파일을 열 때 사용되는 함수로 우선 root directory 부터 열어서 lookup 을 통해서 root directory 내에 해당 name 을 가진 dir_entry 의 존재 여부를 확인하여 inode 에 저장하여 반환하게 된다.

```C
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}
```
마지막으로 특정 Name 을 가진 file을 지울 때 사용하는 함수로, 마찬가지로 root directory 를 여는 것을 시작으로 특정 dir_entry 를 삭제하게 된다.

#### fsutil.c & fsutil.h
위의 코드들을 토대로 정의된 file system을 기반으로 하여 실제 pintos에서 사용할 수 있도록 utility 를 정의해놓은 파일들로 ls, cat, rm 등의 명령어가 입력되면 해당 기능을 수행하도록 지정한 함수들이다.

```C
void
fsutil_ls (char **argv UNUSED) 
{
  struct dir *dir;
  char name[NAME_MAX + 1];
  
  printf ("Files in the root directory:\n");
  dir = dir_open_root ();
  if (dir == NULL)
    PANIC ("root dir open failed");
  while (dir_readdir (dir, name))
    printf ("%s\n", name);
  dir_close (dir);
  printf ("End of listing.\n");
}
```
subdirectory 가 없으므로, root directory 의 directory 를 받아와서 이를 토대로 while 문을 통해서 파일들의 List를 읽어오게 된다.

```C
void
fsutil_cat (char **argv)
{
  const char *file_name = argv[1];
  
  struct file *file;
  char *buffer;

  printf ("Printing '%s' to the console...\n", file_name);
  file = filesys_open (file_name);
  if (file == NULL)
    PANIC ("%s: open failed", file_name);
  buffer = palloc_get_page (PAL_ASSERT);
  for (;;) 
    {
      off_t pos = file_tell (file);
      off_t n = file_read (file, buffer, PGSIZE);
      if (n == 0)
        break;

      hex_dump (pos, buffer, n, true); 
    }
  palloc_free_page (buffer);
  file_close (file);
}
```
hex와 ASCII 코드를 기반으로 ARGV[1] 에 입력된 file_name 을 통해서 file을 찾고 해당 file을 read 해서 파일의 내용을 16진수와 ASCII로 출력하는 함수이다.

```C
void
fsutil_rm (char **argv) 
{
  const char *file_name = argv[1];
  
  printf ("Deleting '%s'...\n", file_name);
  if (!filesys_remove (file_name))
    PANIC ("%s: delete failed\n", file_name);
}
```
ARGV[1] 에 입력된 파일 명을 토대로 디렉토리에서 해당 이름을 가진 파일을 지우는 연산을 수행한다.

```C
void fsutil_extract (char **argv UNUSED) 
```
scratch block device에 저장되어 있는 ustar-format tar achive 읽을 때 사용하는 함수로, 정확히는 Pintos block device type으로 extract 하는 함수이다. 우선 block_get_role 을 통해 scrach device를 열고, 해당 device를 통해서 한 block 씩 계속해서 읽어오게 된다. 읽어온 데이터를 기반으로 header 를 통해서 file_name, type, size를 확인하고 이를 바탕으로 각 데이터를 처리하게 된다. 이렇게 처리된 데이터는 filesys_create를 통해서 각 Block을 file system 에 복사하게 된다. 마지막으로 block deivce의 ustar header 첫 2개의 block 을 지워서 함수가 올바르게 수행될 수 잇도록 하며, end-of-archive marker를 남기는 것으로 마무리된다.

```C
void fsutil_append (char **argv)
```
fsutil_append 는 정반대로 작동하며 현재 file system 에 저장되어 있는 file을 ustar-format 으로 변경하여 scratch device에 append 하게 된다. 이 경우에도 마찬가지로 순서대로 append 해주고, 마지막에는 항상 marker를 남기게 된다. 우선 특정 sector 부터 buffer를 할당하게 된다. filesys_open으로 옮기고자 하는 파일을 일고, sratch_device도 열어주게 된다. 이후에 ustar-format header를 직접 생성하여 file_name, type, size를 기반으로 header를 생성하여 buffer에 저장하게 되고, 이후 이 Buffer에 저장된 Header를 sector에 하나씩 넘겨가면서 저장하게 된다. 마찬가지로 이 경우에도 0 BLock 을 2개를 할당하여 종료를 표시하게 된다.

#### summary of file system
우선 굉장히 많은 file system의 구현에 대해서 살펴보았다. 실제 초반에 pintos document에 있던 limitation들과 연관지어서 다시 정리해보자면, block device의 구현 부분을 봐서 알겠지만, 주석에는 internal sync 가 있다고는 했지만 실제 구현 상 internal sync가 완벽하지 않으므로, concurrent access 시에 방해가 발생할 수 있다는 것을 알 수 있었으며, file size 또한 growth 연산이 정의되어 있지 않기에 생성 시 정해진다는 것 또한 알 수 있었다. 이외에도 root directory 에서 바로 찾아나가는 것처럼 sub directory 로 구현되어 있지 않고, 각 연산 혹은 함수 실행 중간에 문제가 발생할 시 절대 복구할 수 없는 것처럼 보인다. 마지막으로 filesys_remove() 의 구현에서 지우더라도 접근이 가능하다는 것은 바로 inode부터 본다면 inode_remove 에서는 직접 지우는 것이 아닌 Flag 만 설정하게 되고, 마지막으로 읽고 있는 곳에서 inode_close를 실행했을 경우에만 free 해준다. 즉, 읽고 있는 경우에는 곧바로 지워지지 않고 남아있게 되며, 이는 마지막 프로세스가 다 읽을 때 까지 유지된다는 것을 구현을 통해서 함께 알 수 있다.

#### brief analysis of pintos-mkdisk
위의 pintos document 에서 초기에 file system을 저장하기 위한 가상의 system 을 지정하기 위해서 pintos-mkdisk 를 통해서 2 MB 의 가상 디스크를 생성하는데 해당 과정을 간략하게 살펴보고자 한다. pintos-mkdir 는 말 그대로 가상 디스크를 생성하고 설정하는 과정으로 디스크 파일을 생성하고, 파티션을 나누며, 부트로더 까지 포함할 수 있도록 구현되어 있다.

```perl
BEGIN { my $self = $0; $self =~ s%/+[^/]*$%%; require "$self/Pintos.pm"; }
our ($disk_fn);            # Output disk file name.
our (%parts);              # Partitions.
our ($format);             # "partitioned" (default) or "raw"
our (%geometry);           # IDE disk geometry.
our ($align);              # Align partitions on cylinders?
our ($loader_fn);          # File name of loader.
our ($include_loader);     # Include loader?
our (@kernel_args);        # Kernel arguments.
```
첫 줄의 경우 perl 모듈을 불러오는 과정을 통해서 pintos 에서 사용될 함수들을 불러오게 되며, 밑의 Our 의 경우 전역변수를 선언하게 된다.

``` perl
if (grep ($_ eq '--', @ARGV)) {
    @kernel_args = @ARGV;
    @ARGV = ();
    while ((my $arg = shift (@kernel_args)) ne '--') {
	push (@ARGV, $arg);
    }
}
GetOptions ("h|help" => sub { usage (0); }, ... )
  or exit 1;
usage (1) if @ARGV != 1;
```
입력받은 argument 를 Parsing 하는 부분으로 kernel_args에 저장하게 되고, 각 option 별 sub routine을 지정하게 된다.

```perl
$disk_fn = $ARGV[0];
die "$disk_fn: already exists\n" if -e $disk_fn;

# Open disk.
my ($disk_handle);
open ($disk_handle, '>', $disk_fn) or die "$disk_fn: create: $!\n";
```
첫 번째 argument로 입력받은 부분인 $ARGV[0] 을 disk_fn 에 저장하여 disk_fn 이라는 이름을 가진 disk 를 생성하게 된다.

```perl
# Sets the loader to copy to the MBR.
sub set_loader {
    die "can't specify both --loader and --no-loader\n"
      if defined ($include_loader) && !$include_loader;
    $include_loader = 1;
    $loader_fn = $_[1] if $_[1] ne '';
}

# Disables copying a loader to the MBR.
sub set_no_loader {
    die "can't specify both --loader and --no-loader\n"
      if defined ($include_loader) && $include_loader;
    $include_loader = 0;
}

# Figure out whether to include a loader.
$include_loader = exists ($parts{KERNEL}) && $format eq 'partitioned'
  if !defined ($include_loader);
die "can't write loader to raw disk\n" if $include_loader && $format eq 'raw';
die "can't write command-line arguments without --loader or --kernel\n"
  if @kernel_args && !$include_loader;
print STDERR "warning: --loader only makes sense without --kernel "
  . "if this disk will be used to load a kernel from another disk\n"
  if $include_loader && !exists ($parts{KERNEL});
```
각 함수는 boot loader와 관련된 함수로 set_loader의 경우 boot loader의 포함 여부를 설정하게 되며, set_no_loader 함수의 경우 Boot loader를 포함하지 않도록 설정하게 된다.

```perl
# Read loader.
my ($loader);
$loader = read_loader ($loader_fn) if $include_loader;

# Write disk.
my (%disk) = %parts;
$disk{DISK} = $disk_fn;
$disk{HANDLE} = $disk_handle;
$disk{ALIGN} = $align;
$disk{GEOMETRY} = %geometry;
$disk{FORMAT} = $format;
$disk{LOADER} = $loader;
$disk{ARGS} = \@kernel_args;
assemble_disk (%disk);

# Done.
exit 0;
```
마지막으로 %disk 에 각 데이터를 저장하게 되며, assemble_disk 함수를 통해서 실제 disk 를 생성하는 것으로 실행을 마무리하게 된다.

### Install User Program in Pintos
위의 과정들을 통해서 Pintos 내에서 간단하게나마 어떻게 File system이 구현되었는지 알 수 있었고, 어떻게 관리되고 있는지도 알 수 있었다. 다시 pintos 문서로 돌아가면 pintos-mkdisk 를 통해 가상의 디스크가 생성되는 것 까지는 이해를 할 수 있었지만, 그 다음 명령어인 pintos -f -p 는 어떻게 실행되는지 아직 모르고 있는 상황이다. pintos 가 실행되고 어떻게 argument 가 전달되어 실행되는지 일련으 과정을 알아보면서 어떻게 file system에서 formatting 과 copy 등등이 실행될 수 있는지의 과정을 함께 살펴보고자 한다.

#### init.c
```C
int
main (void)
{
  char **argv;

  argv = read_command_line ();
  argv = parse_options (argv);

#ifdef USERPROG
  tss_init ();
  gdt_init ();
#endif

  ...

#ifdef USERPROG
  exception_init ();
  syscall_init ();
#endif

  ...

#ifdef FILESYS
  ide_init ();
  locate_block_devices ();
  filesys_init (format_filesys);
#endif

  ...

  run_actions (argv);
  
  ...
}
```
pintos가 부팅되는 과정을 아주 간단하게 요약하자면, perl로 구현되어 있는 pintos가 실행되면서 find_disks() 를 통해 디스크에서 커널을 찾고 초기 설정을 하게 되고, 이후에 loader.S 로 넘어가 실행을 이어가게 된다. find_disks() 내에서 kernel_args를 넘겨받아 args에 저장하게 되고, 이후 디스크로 넘겨주게 되면서 loader.S를 실행하고, 이후 차례로 Start.S 로 넘어가 실행하게 되면서 call main 을 통해 드디어 init.c 의 main () 함수를 실행하게 된다. 우선 여기서 argv 에는 disk 에 넣었던 kernel_agrs 들이 저장되어 있게 되며 read_command_line() 과 parse_options() 를 통해 실행을 이어가게 된다.

```C
static char **
read_command_line (void) 
{
  static char *argv[LOADER_ARGS_LEN / 2 + 1];
  char *p, *end;
  int argc;
  int i;

  argc = *(uint32_t *) ptov (LOADER_ARG_CNT);
  p = ptov (LOADER_ARGS);
  end = p + LOADER_ARGS_LEN;
  for (i = 0; i < argc; i++) 
    {
      if (p >= end)
        PANIC ("command line arguments overflow");

      argv[i] = p;
      p += strnlen (p, end - p) + 1;
    }
  argv[argc] = NULL;

  /* Print kernel command line. */
  printf ("Kernel command line:");
  for (i = 0; i < argc; i++)
    if (strchr (argv[i], ' ') == NULL)
      printf (" %s", argv[i]);
    else
      printf (" '%s'", argv[i]);
  printf ("\n");

  return argv;
}
```
kernel command line을 입력받아서 이를 word로 나누고 words를 argv 처럼 나눠서 array 형태로 반환하게 되는 과정으로 이루어져 있다. 이때 ptov 는 physical 주소를 가상 주소로 바꿔주는 함수로 loader.h 에 저장되어 있는 LOADER_ARGS를 바탕으로 virtual address 로 변경하여 반환하게 된다. 즉 argument 들은 실제 disk 즉, physical memory 상에 저장되어 있는데 이를 읽어오기 위해서는 virtual address 를 통해서 읽어올 필요성이 있다. 따라서 virtual address 로 변환하는 과정을 거친 이후에 command line 에 포함된 argument 의 개수를 세고 각 Word 로 나눠서 연산을 수행하여 각 agrument를 array 형태로 저장하여 반환하게 된다.

```C
static char **
parse_options (char **argv) 
{
  for (; *argv != NULL && **argv == '-'; argv++)
    {
      char *save_ptr;
      char *name = strtok_r (*argv, "=", &save_ptr);
      char *value = strtok_r (NULL, "", &save_ptr);
      
      if (!strcmp (name, "-h"))
        usage ();
      else if (!strcmp (name, "-q"))
        shutdown_configure (SHUTDOWN_POWER_OFF);
      else if (!strcmp (name, "-r"))
        shutdown_configure (SHUTDOWN_REBOOT);
#ifdef FILESYS
      else if (!strcmp (name, "-f"))
        format_filesys = true;
      else if (!strcmp (name, "-filesys"))
        filesys_bdev_name = value;
      else if (!strcmp (name, "-scratch"))
        scratch_bdev_name = value;
#ifdef VM
      else if (!strcmp (name, "-swap"))
        swap_bdev_name = value;
#endif
#endif
      else if (!strcmp (name, "-rs"))
        random_init (atoi (value));
      else if (!strcmp (name, "-mlfqs"))
        thread_mlfqs = true;
#ifdef USERPROG
      else if (!strcmp (name, "-ul"))
        user_page_limit = atoi (value);
#endif
      else
        PANIC ("unknown option `%s' (use -h for help)", name);
    }
  random_init (rtc_get_time ());
  
  return argv;
}
```
위에서 virtual address로 변환하여 array 형태로 저장된 arguments 를 받아와서 이를 토대로 각 Option 별로 할당을 진행하게 된다. 우선 for 문으로 반복하여 -로 시작하는 argument 일 경우 -option=value 형태이므로 option, value를 각각 분할하여 연산을 수행하게 된다. Strcmp 함수를 통해 Option 을 확인하여 해당하는 option 일 경우 flag를 설정하거나 value에 맞게 값을 설정하게 된다. 각 옵션별로 간단히 다음과 같은 연산을 수행한다. -h (도움말 출력), -q (커널 종료 시 전원 끄기), -r (커널 종료 시 재부팅), -f (파일 시스템 포맷), -filesys / -scratch (file system과 scractch disk의 block device의 name 설정), -swap (swap block device 이름 설정), -rs (random number generator 초기화), -mlfqs (multi-level feedback queue scheduling), -ul (user grogram 의 page limit을 설정) 해주게 된다. 이후 -rs 옵션이 설정된 경우 value로 seed를 초기화해주지만 -rs 가 설정되지 않은 경우 rtc_get_time 을 통해서 Seed를 설정하게 된다.

```C
static void
run_actions (char **argv) 
{
  /* An action. */
  struct action 
    {
      char *name;                       /* Action name. */
      int argc;                         /* # of args, including action name. */
      void (*function) (char **argv);   /* Function to execute action. */
    };

  /* Table of supported actions. */
  static const struct action actions[] = 
    {
      {"run", 2, run_task},
#ifdef FILESYS
      {"ls", 1, fsutil_ls},
      {"cat", 2, fsutil_cat},
      {"rm", 2, fsutil_rm},
      {"extract", 1, fsutil_extract},
      {"append", 2, fsutil_append},
#endif
      {NULL, 0, NULL},
    };

  while (*argv != NULL)
    {
      const struct action *a;
      int i;

      /* Find action name. */
      for (a = actions; ; a++)
        if (a->name == NULL)
          PANIC ("unknown action `%s' (use -h for help)", *argv);
        else if (!strcmp (*argv, a->name))
          break;

      /* Check for required arguments. */
      for (i = 1; i < a->argc; i++)
        if (argv[i] == NULL)
          PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

      /* Invoke action and advance. */
      a->function (argv);
      argv += a->argc;
    }
}
```
우선 struct action 을 선언하여 각 action의 name, argc (argument 개수), funcion (실행할 함수)를 저장하게 되며, 각 action 을 지정하여 실행하게 된다. 이를 바탕으로 actions 배열을 선언하였는데, 각 name 에 해당하는 action 을 실행하기 위한 argument 의 개수와 실행할 함수를 선언하게 되어있다. 특정 user program 을 실행할 때 사용하는 run 명령어의 경우 run 을 포함하여 한 개 즉, 실행할 program의 이름이 필요하고, 이는 run_task 에 전달되어 실행되는 것을 알 수 있으며, 앞서 알아봤던 file system을 관리하기 위한 ls, cat, rm, extract, append가 모두 fsutil_ls, fsutil_cat, fsutil_rm, fsutil_extract, fsutil_append 을 통해 실행되는 것을 알 수 있었다. 이후에 while 문을 통해서 계속 argv를 순회하면서 action 과 argument 를 짝으로 맞춰서 실행을 반복해서 모든 argument 가 처리될 때 까지 실행을 반복하게 된다.

```C
static void
run_task (char **argv)
{
  const char *task = argv[1];
  
  printf ("Executing '%s':\n", task);
#ifdef USERPROG
  process_wait (process_execute (task));
#else
  run_test (task);
#endif
  printf ("Execution of '%s' complete.\n", task);
}
```
run_task 는 위에서 인자가 2개가 필요하다고 했으므로, argv[1] 에 저장된 인자를 바탕으로 task 의 이름을 불러오게 된다. 시작하기 이전에 task 의 이름을 출력하고 시작하게 된다. 이후 USERPROG context 내에서 실행되는 경우 Process_wait (process_execute (task)) 를 통해 실행을 시작하게 된다. process_execute (task) 를 통해 task 를 실행하게 되고, wait 함수가 실행이 종료될 때까지 기다리도록 작동하게 된다. userprog가 아닐 시 run_test를 통해서 테스트 용도로 작업을 실행하게 되며, 작업이 완료된다면 Printf를 통해서 실행된 task 를 출력하고 마무리하게 된다. 

```C
static void
locate_block_devices (void)
{
  locate_block_device (BLOCK_FILESYS, filesys_bdev_name);
  locate_block_device (BLOCK_SCRATCH, scratch_bdev_name);
#ifdef VM
  locate_block_device (BLOCK_SWAP, swap_bdev_name);
#endif
}
```
locate_block_device는 file system 을 위한 block device를 할당하고, 이후 scratch 를 위한 block device를 할당하게 되는 과정으로 실행된다. 즉, 특정 role에 맞는 device에 Name을 알맞게 부여하거나 혹은 이름에 맞는 block device를 찾아서 해당 역할을 부여하게 된다.

```C
static void
locate_block_device (enum block_type role, const char *name)
{
  struct block *block = NULL;

  if (name != NULL)
    {
      block = block_get_by_name (name);
      if (block == NULL)
        PANIC ("No such block device \"%s\"", name);
    }
  else
    {
      for (block = block_first (); block != NULL; block = block_next (block))
        if (block_type (block) == role)
          break;
    }

  if (block != NULL)
    {
      printf ("%s: using %s\n", block_type_name (role), block_name (block));
      block_set_role (role, block);
    }
}
```
특정 role 에 맞는 block device를 찾아서 반환하는 함수로 이름이 같은 block 을 찾거나 role 에 맞는 block 중에 가장 첫 번째로 찾은 block 을 반환하게 된다. 우선 이름이 같은 것을 찾았다면 그걸 반환하게 되고, 만약 이름이 입력되지 않은 경우에는 block list에서 같은 role을 지니고 있는 block 을 반환하게 된다. 반약 블록을 찾았다면 prinf 를 통해서 state를 출력하고, set_role을 통해서 찾은 block 에 role을 부여하게 된다.

*** IDE and ide_init ***
```C
void ide_init (void) 
```
main() 에 사용된 다른 한 함수는 ide_init 이다. 해당 함수는 integrated drive electronics 즉 IDE disk를 초기화하는 함수로 IDE disk device를 사용하게 하는 함수이다. 우선적으로 Advanced Technology Attachment 를 초기화하여 PINTOS가 File system을 지원하게 된다. IDE는 disk 와 CPU 사이의 Interface 로 ide_init 함수 내에서는 ATA 를 세팅하기 위한 인터페이스이다. 우선 IDE는 총 2개의 primary와 secondary channel 로 나뉘게 되는데 각 채널별로 Master-Slave를 이루게 된다. 우선 각 Channel을 reset하여 interrupt와 Register를 설정하게 된다. 다음으로 각 channel 에 연결되어 있는 disk 를 초기화하며 마찬가지로 새롭게 Name과 number를 설정하게 된다. 이후에 interrupt handler를 추가하고 channel을 초기화하게 된다. 이를 통해서 ATA 를 활용할 수 있도록 disk 를 설정하게 된다.

#### process.c & process.h
이전 main()에서 run_task가 실행되는 과정에서 실행의 흐름을 따라가면서 run_task 에서 실행된 Process_execute 가 어떻게 실행이 되는지 이어서 살펴보고자 한다.

```C
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  return tid;
}
```
file_name 을 입력으로 받아 어떻게 해당 user program 을 실행하게 되는지 살펴보자. 우선 새로운 thread를 실행하게 되는데 thread의 이름을 file_name으로 설정했으며, start_process 를 실행시키도록 thread를 생성하게 된다. process_execute 함수 내에서 thread를 생성하는 것이므로, process_execute 가 종료되기 전에 이미 해당 thread가 스케쥴되어 실행될 수 있거나 혹은 아예 exit 까지도 수행되고 나서 return tid로 종료될 수 있는 가능성이 존재한다. 그렇지만 반환은 일단 새로 생성한 tid를 반환하게 된다. 조금 더 구체적으로 살펴보면 file_name 이라는 변수를 아예 fn_copy라는 변수에 아예 복사를 하게 되는데 caller 즉 현재 함수랑 이후 실행될 load() 에서 동시에 file_name 을 참고하게 되는데 이러면 Race condition 에 빠질 수 있으므로 복사를 하고 실행하게 된다.

```C
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}
```
위에서 정의된 thread 함수인데, 새로운 user program 을 load하고 실행하는 과정을 수행하는 함수이다. 하나씩 살펴보자면 위에서 fn_copy 를 통해 복사한 입력인 file_name_을 입력받게 된다. 우선, intr_frame if_ 를 초기화하고, 각 register인 gs, fs, es, ds, ss 를 SEL_UDSEG로 설정하여 user code 가 접근하는 공간으로 설정하게 된다. 마찬가지로 cs 또한 UCSEG로 설정하여 사용자 코드가 실행되도록 한다. 이후 인터럽트 활성화를 위한 FLAG_IF와 MBS 를 설정하고 load 함수를 통해서 user program 을 memeory 로 load 하고 eip 에 user program 이 실행될 주소와 esp 에 stack pointer를 지정하며 실행을 시작할 준비를 마치게 된다. 준비가 완료되었다면 마지막 어셈블리 루틴을 잘 확인해야 하는데, 마치 인터럽트에서 반환된 것처럼 User program 을 실행하게 된다. 우선 intf_frame에 무언가가 들어있는 척 movl %0, %%esp 를 통해서 esp 를 &if_ 로 설정하고, 이후 jmp intr_exit 를 통해 if_.eip 로 넘어가면서 실행을 시작하게 되고, 아예 그 밑인 NOT_REACHED로는 도달하지 않게 된다.

```C
int
process_wait (tid_t child_tid UNUSED) 
{
  return -1;
}
```
아직 구현되어 있지 않은 상황이지만, 실제 구현 시에는 자식 thread가 종료될 때까지 기다린 뒤에 자식 thread가 종료되는 상태를 반환하게 되는 것이 핵심이다. 만약 강제로 종료된 경우거나 중복 호출, 혹은 문제가 있는 경우에는 -1을 반환하게 되며 부모 -자식 관계가 아닌 경우나 유효하지 않은 TID 인 경우 등을 포함한다. 

```C
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  pd = cur->pagedir;
  if (pd != NULL) 
    {
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}
```
현재 실행되고 있는 프로세스를 종료하고 할당된 resources 를 해제하게 된다. current process의 page directory를 지우게 되면서 kernel-only page directory 로 전환하게 되면서 user program을 위한 page directory 를 삭제하게 된다. 해당 과정은 우선 cur_pagedir 를 NULL로 바꿔주는데 이를 통해서 timer interrupt 발생 시에도 process의 page directory 로 돌아오지 못하도록 한다. 또한, padedir_activate 를 통해서 kernel-only page directory를 Activate 한 뒤에야 current process의 page directory를 destroy 하게 되는 순서로 구현해야 하며 그렇게 구현되어 있다. 순서가 바뀌게 된다면, 자꾸만 User program 의 Page directory를 activate 하게 되거나, 이미 지워진 directory 를 읽게 되는 문제가 발생할 수 있다. 

```C
void
process_activate (void)
{
  struct thread *t = thread_current ();

  pagedir_activate (t->pagedir);

  tss_update ();
}
```
현재 실행되는 current thread에 맞춰서 CPU 가 실행될 수 있도록 설정해주는 함수로 context_switch 가 발생할 때마다 실행하게 된다. 우선 Pagedir_activate 를 통해서 현재 thread의 Pade table을 활성화하게 된다. 이후 Task State Segment 도 업데이트를 통해서 interrupt context 상에서 활용할 TSS 를 준비하게 된다. 

```C
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */
```
이제 Load 가 어떻게 구현이 되어 실행되는지 확인하기 이전에 우선 ELF 에 대해서 먼저 확인을 해야한다. ELF 란 Executable and Linkable Format Binary 를 Load하기 위한 파일 형식을 담고 있다. 대부분 ELF1 에 포함된 정의들이고 위에서부터 확인해보도록 하자. Elf32_word, Addr, Off 는 각각 ELF 에서 사용되는 32비트 정수형 데이터셋이고 Half는 16비트 정수형 데이터셋이다. 또한, printf를 위해서 ELF의 출력을 위한 정의로 hexadecimal로 출력하기 위한 데이터 타입을 담고 있다. 그 다음에 등장하는 Elf32_Ehdr는 ELF 파일의 가장 처음에 위치하는 데이터를 표현하기 위한 struct로 ELF 의 가장 처음에 존재하는 파일로 ELF 파일의 정보를 담고 있는 Struct 이다. Elf32_Phdr은 ELF의 헤더를 표현하기 위한 struct 로 파일의 e_phoff 에서 시작하여 e_phnum 개수 만큼의 header가 존재한다. 마지막으로 p_type 은 각각 LOAD 여부, DYNAMIC 여부, 등등을 나타내기 위해 정의되었으며, p_flags는 권한 XWR을 지정하기 위한 flag 로 작동한다.

```C
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  if (!setup_stack (esp))
    goto done;

  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  file_close (file);
  return success;
}
```
이제 위에서 정의된 ELF 타입들을 통해서 ELF file 을 Load하여 thread에 직접 Load하고 실행할 수 있도록 초기화하는 역할을 담당한다. 크게 보면 memory 에 Load한 ELF file 의 Start address를 eip 에 지정하고, 마찬가지로 Stack pointer를 esp 에 지정하도록 한다. 구체적으로 살펴보면, 현재 thread t 와 ehdr (ELF header), file (ELF header 를 file 에 저장하기 위한 변수) 를 우선 선언하게 된다. 이후 thread t 가 실행되기 위한 padedir 을 할당하게 되고, process_activate 를 통해서 pagedir 를 이용할 수 있도록 한다. 즉 각 process 마다 다른 address space를 할당하게 되고, 이후 ELF file 을 드디어 읽어오게 된다. file system device를 통해서 해당 file name을 가진 file을 읽어와서 file_read 함수를 통해 해당 파일을 확인하게 된다. 이 과정 중에서 ehdr 에 header의 내용을 저장하고, ELF 파일 형식과 실행 파일 등을 조건에 맞게 확인하게 된다. 이후 program header를 file_ofs에 저장하고 각 header를 for 문에서 순회하면서 E_phnum 개수만큼 확인하게 된다. 이 때 file_seek 를 통해서 다시 Pos 를 조정하고 다시 file_read 로 읽고를 반복하면서 각 header를 전부 읽는다. 이 때 읽어온 header를 기존에 정의된 enum 에 따라서 이 중 PT_LOAD 의 경우 load 가 가능하므로 이 경우에만 드디어 데이터를 읽어오게 된다. 이후 file 에서 데이터를 읽고, 남은 영역에 대해서는 zero 로 덮어쓰게 된다. 또한, Load_segment 함수를 통해서 file data를 laod 한 뒤에 mem_page를 zero 로 덮어쓰게 된다. 마지막으로 setup_stack 을 통해 stack 을 설정하고 eip 를 ehdr.e_entry 로 start address를 할당하면서 마무리하게 된다. 이제 Load 에서 사용된 세부 함수들을 몇 가지 더 살펴보고자 한다.

```C
static bool validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
```
phdr 이 valid 한지 여부를 확인하는 함수로, loadable segment 가 file 내에 존재하는지 확인하고 문제가 없으면서 존재하는 경우에만 true를 반환한다.

```C
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}
```
load 에서 PT_LOAD 인 경우에만 Load를 하게 되었는데 이 때 사용하던 함수가 바로 load_segment 이다. 우선 file의 특정 위치인 offset에서부터 READ_BYTES만큼 read 하여 current processor의 virtual address space 에 Load 하는 역할을 담당하고 있다. 즉, READ_BYTES 부분 만큼은 읽어오게 되고, 나머지 ZERO_BYTES 부분에서 0으로 초기화를 진행하게 된다. 또한, file_seek를 통해서 pos 를 ofs 로 바꿔줘 다시 해당 offset 부터 읽어올 수 있도록 한다. 각 page 별로 read_bytes와 zero_bytes를 지정하여 남은 공간에는 0으로 초기화하도록 작동한다. 이 때 각 페이지를 읽어올 때마다 Kpage라는 변수에 새로운 페이지를 palloc 하고 이 페이지에 읽어온 데이터와 zero padding 을 추가하는 방식으로 kpage를 완성한다. 해당 과정을 반복하여 입력으로 받은 read_bytes와 zero_bytes를 만족할 때까지 실행하게 되면서 결국 user program을 memory 로 읽어오게 된다.

```C
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}
```
esp 를 설정할 때 사용하는 함수로 user virtual memory의 top에 0으로 초기화된 page를 할당하는 방식으로 설정된다. install_page 함수를 실행하여 user stack을 kernel page에 할당하게 되고 esp는 PHYS_BASE로 지정되어 Stack pointer를 반환하게 된다. 이를 통해서 top 부터 메모리를 PGSIZE 까지 사용할 수 있도록 할당되는 방식이다.

```C
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
```
virtual address (UPAGE)와 kernel virtual address(KPAGE) 사이의 mapping 을 page table에 추가하는 함수이다. writable 이 설정되어 있다면 USEr process는 해당 page를 수정할 수 있는 권한이 있으며, UPAGE는 이미 Mapping 되어 있으면 안된다. 또한, KPAGE의 경우에는 user pool 에서 Obtained 되어야 하며, 성공할 경우 True를 반환하게 된다.

이렇게 pintos가 부팅되면서 어떻게 file system이 구성되고, 그렇게 구성된 file system 을 바탕으로 init.c 에서 main 함수를 통해 argument가 넘어가면서 실제 process 단위로 어떤 과정을 거쳐서 user program 이 실행되는지를 확인하였다. 하지만, 아직 확인하지 못한 부분이 남았다. 바로 TSS와 GDT 이다. 둘 다 main 함수 실행 시 초기화되면서 각 메모리를 보호하고 인터럽트 발생 시 어떻게 작동되는지 이해할 수 있는 중요한 부분이기에 추가적인 분석이 필요하다. 

#### tss.c & tss.h
TSS는 Task-State Segment 로 process의 multitasking을 지원하기 위한 Struct 이다. 기존에는 멀티태스킹을 위한 것으로, Task 별로 Register, Stack, Segment를 저장하게 된다. 하지만 실제로는 SW-level에서 Multitasking이 구현되어 있어서 TSS는 interrupt 발생 시에 사용되는 목적으로 남겨져 있다. user program 에서 Interrupt가 발생하는 시에 TSS의 ss0, esp0에 저장된 정보를 바탕으로 바탕으로 Kernel stack 으로 바로 전환하게 되면서 이를 통해서 user stack 이 아닌 안전한 kernel stack 에서 interrupt를 작동하게 된다. 추가적으로 kernel stack에서 interrupt가 발생할 경우에는 추가적인 switch가 필요하지 않게 된다. 따라서 TSS는 ss0과 esp0에 새로운 thread의 Kernel stack address가 반영되도록 구현되었으며, 실제 context switch 시에는 Thread_schedule_tail() 에서 TSS의 stack pointer를 새로운 thread의 kernel stack address가 되도록 setting 하여 잘 switch가 작동하도록 설정해야 한다.

```C
struct tss
  {
    uint16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    uint16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    uint16_t ss1, :16;
    void *esp2;
    uint16_t ss2, :16;
    uint32_t cr3;
    void (*eip) (void);
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint16_t es, :16;
    uint16_t cs, :16;
    uint16_t ss, :16;
    uint16_t ds, :16;
    uint16_t fs, :16;
    uint16_t gs, :16;
    uint16_t ldt, :16;
    uint16_t trace, bitmap;
  };

/* Kernel TSS. */
static struct tss *tss;
```
TSS 를 위한 struct로 call gate, task gate를 위한 switch 발생 시에 필요한 Data들을 포함하게 된다. 다른 task의 TSSㄹ를 저장하는 Back_link, kernel stack을 저장하는 esp0, ss과 나머지 실제 사용되지 않지만 기존 TSS에 존재하는 데이터로 구성되어 있다.

```C
void
tss_init (void) 
{
  tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  tss->ss0 = SEL_KDSEG;
  tss->bitmap = 0xdfff;
  tss_update ();
}
```
TSS를 설정하는 함수로 pintos 에서 구현된 TSS는 call gate나 task gate와 같이 실행되는 것이 아니기 때문에 일부 데이터(ss0, esp0, bitmap)만 저장하고 사용하게 된다. 따라서 tss_init 에서는 ss0 에는 SEL_KDSEG(kernel data selector)를 할당하고 tss_update를 통해서 esp0 을 초기화하게 된다.

```C
struct tss *
tss_get (void) 
{
  ASSERT (tss != NULL);
  return tss;
}
```
현재 TSS 를 반환한다.

```C
void
tss_update (void) 
{
  ASSERT (tss != NULL);
  tss->esp0 = (uint8_t *) thread_current () + PGSIZE;
}
```
TSS의 esp0 을 업데이트하는 함수로 현재 thread의 kernel_stack의 끝을 지정하도록 하여 interrupt가 발생하여 Kernel mode로 Switch 시에 kernel stack 이 사용되게 지정한다.

#### gdt.h & gdt.c
GDT는 Global Descriptor Table의 약자로 memory segment 에 관한 정보를 담고 있으며, 시스템 내의 모든 프로세서가 사용할 수 있는 segment 를 정의하고 있다. 또한 per-processor 단위의 LDT도 존재하긴 하지만 현대의 OS에서는 잘 사용되지 않는다. GDT의 각 entry 에서는 byte offset을 기준으로 구별되며, 각 byte offset이 segment를 의미한다. segment는 code, data, TSS 크게 3가지로 구성되어 있다.

```C
void
gdt_init (void)
{
  uint64_t gdtr_operand;

  gdt[SEL_NULL / sizeof *gdt] = 0;
  gdt[SEL_KCSEG / sizeof *gdt] = make_code_desc (0);
  gdt[SEL_KDSEG / sizeof *gdt] = make_data_desc (0);
  gdt[SEL_UCSEG / sizeof *gdt] = make_code_desc (3);
  gdt[SEL_UDSEG / sizeof *gdt] = make_data_desc (3);
  gdt[SEL_TSS / sizeof *gdt] = make_tss_desc (tss_get ());

  gdtr_operand = make_gdtr_operand (sizeof gdt - 1, gdt);
  asm volatile ("lgdt %0" : : "m" (gdtr_operand));
  asm volatile ("ltr %w0" : : "q" (SEL_TSS));
}
```
GDT의 경우 array 형태로 관리되며, DPL 이 0인 경우 kernel 의 segment를 의미하고, DPL이 3인 경우 user program 의 Segment를 의미하게 된다.

User Program의 실행 과정을 위의 구현에 따라 정리해보고자 한다. ELF format의 User program을 초기에 pintos-mkdisk 통해서 생성한 가상의 disk에 load하여 실행시킬 수 있을 것이다. 이 때 load되는 User program은 Memory 에 들어갈 수 있을 만큼 용량이 작아야 하면서도, 아직 Syscall 이 구현되어 있지 않으므로 malloc()과 같은 연산은 실행하기 어려울 것이며, 이외에도 floating point 연산을 switching thread 시에 kernel에서 저장하지 않으므로 FP 연산이 지원되지 않을 것이다. 우선 User Program 이 위의 조건을 만족한다면, compiler와 linker를 사용하여 Pintos에서 실행을 위한 80x86 ELF executable을 생성할 수 있게 된다. 위 특성들을 바탕으로 볼 때 Makefile을 통해 User Program을 ELF execuatble 형식으로 컴파일을 진행하고, 이후에 virtual disk에 해당 컴파일된 프로그램을 복사하게 된다. 이후, pintos가 부팅하는 과정에서 virtual disk에서 file system을 불러오게 되는데, 이 과정에서 executable 한 User program을 발견하게 된다면, 해당 프로그램을 메모리에 올리기 위해 process_execute() 를 통해 메모리에서 실행하게 된다. 이후 file system 에서 ELF execuatble 을 읽어와서 memory에 Code, Data, BSS, Stack 등이 저장되고, 각 segment는 virtual address space 내에 mapping 된다. 이후 stack 까지 설정이 완료되면, intr_exit() 을 통해 마치 인터럽트가 종료된 것처럼 꾸미면서 entry point로 들어가며 실행이 시작되게 된다.

### Virtual Memory Management
pintos에서는 vritual memory를 크게 2가지 영역으로 나누어 구성되어 있다. 첫 번째 영역은 virtual address 0부터 PHYS_BASE 까지 할당되어 있으며 PHYS_BASE는 vaddr.h에 선언되어 있는 상수로 3 GB 의 공간으로 할당되어 있다. 나머지 영역이 Kernel Memory로 할당되어 있으며, PHYS_BASE 부터 4 GB 까지가 Kernel memeory 로 사용되게 된다. 실제 구현을 살펴보게 되면, PHYS_BASE를 기준으로 낮은 영역은 User Memeory, 큰 영역은 Kernel Memory로 사용되며 Kernel Memory는 실제 Physical Memory와 1 대 1 대응이 되며 PHYS_BASE를 더하고 빼는 것만으로도 physical memory와 Virtual memory를 변환할 수 있으며, Physical Memory의 사이즈와 Kernel Memory의 Size가 같다는 것을 의미함을 알 수 있다. 또한, User Program에서는 PHYS_BASE 이하의 영역에는 자유롭게 접근이 가능하지만, 그 이상의 영역인 Kernel memory 에 접근하게 될 경우 Page Fault() 와 같은 문제가 발생할 수 있게 된다. 반면에 Kernel 에서는 모든 Virtual Memory Space에 자유롭게 접근할 수 있다. 이외에도 User Memory가 Mapping 되지 않은 경우에 해당 Virtual Memory Address에 접근 시에도 Page_Fault()가 발생하게 된다.

일반적으로 virtual memory layout은 일반적으로 0x08048000 부터 시작하여 Code Segment, Initialized Data Segment, Uninitalized Data Segment, User Stack 순서대로 저장되어 있다. Stack의 경우에는 PHYS_BASE부터 시작하여 주소가 내려가는 방향으로 Stack이 커지게 된다. 이번 프로젝트에서는 고정된 stack size만 고려하게 된다. 실제 이렇게 user virtual memeory space를 구성하는 코드는 위에el서 분석한 process.c 의 load() 함수 내에서 이뤄지고 있으며, ELF header를 참고하여 Code 와 Data Segment를 Memory에 배치하게 되는데, load_segment()를 통해서 각 ELF header 에 따라서 User Memory에 배치하게 된다. 이러한 Memory Layout 자체는 Linker Script에 의해서 결정되며 해당 Segment를 적절하게 배치하게 된다. 

또한, system call 시에 User Memory 관리를 위해서 올바른 주소로 접근할 수 있도록 제한해야 한다. 예를 들어 system call 중에 NULL pointer, 혹은 mapping 되지 않은 memory address, kernel memory pointer 등등 제한된 영역에 접근하려 하거나 접근이 금지된 영역에 접근을 하려고 할 때 이를 방지해야 한다. 따라서 접근을 허용하기 이전에 두 가지 방법을 통해서 User Memory를 안전하게 사용할 수 있도록 해야한다. 첫 번째 방법은 vaddr.h 에 정의되어 있는 is_user_vaddr() 나, 혹은 padedir에 있는 함수를 활용해 access가 가능한지 여부를 미리 확인하고, 확인이 끝난 이후에 access 하는 과정으로 구현할 수 있다. 이 구현의 경우 약간의 Overhead가 추가되므로 속도 저하를 야기할 수 있다. 다른 방법으로는 Pointer가 전체가 유효한지 확인하지 말고 단순히 PHYS_BASE 이하인지 여부만 우선적으로 확인하고, 그 이후에 page_fault를 발생시켜 exception 처리를 모두 Page_Fault() 를 통해 구현할 수 있으며, 이 경우 MMU를 통해서 보다 빠른 구현이 가능하지만, Exception handler로 인한 overhead가 존재하게 된다. 접근 이후에도 잘못된 메모리로 확인된 경우 Lock을 해제하거나, 혹은 Memory 를 free하여 Memory leakage를 방지해야 한다.

### System Call Handling Procedure in Pintos
```C
/* From lib/user/syscall.c */
pid_t
exec (const char *file)
{
  return (pid_t) syscall1 (SYS_EXEC, file);
}
```
이번 프로젝트에서는 `halt()` 부터 `close()`까지의 시스템 호출을 구현해야 한다.
이를 위해 먼저 Pintos에서 제공된 시스템 호출 함수들의 정의를 살펴보자. 시스템 
호출 함수들의 정의는 각기 크게 다르지 않으므로, Pintos의 시스템 호출 과정을 
보여주는 예시로 `exec()` 함수를 발췌하였다.

`exec()`의 정의를 보면, `syscall1()`이라는 매크로의 인자로 `SYS_EXEC`과 원래 
인자인 `file`을 넘기며 이를 호출하는 것을 볼 수 있다. 그리고 이의 반환값을
`pid_t` 형으로 형변환(casting)하여 다시 이를 반환하는 것을 볼 수 있다.

```C
/* From lib/syscall-nr.h */
/* System call numbers. */
enum 
  {
    /* Projects 2 and later. */
    SYS_HALT,                   /* Halt the operating system. */
    SYS_EXIT,                   /* Terminate this process. */
    SYS_EXEC,                   /* Start another process. */

    ...

    SYS_INUMBER                 /* Returns the inode number for a fd. */
  };
```
그렇다면, 이 매크로의 인자인 `SYS_EXEC`은 무엇일까? Pintos에서는 시스템 호출마다 
번호를 붙여 시스템 호출 핸들러가 시스템 호출의 종류를 알 수 있도록 한다. 이
상수, 혹은 열거형의 정의는 `lib/syscall-nr.h`에서 찾아볼 수 있으며, `SYS_EXEC`은 
`exec()` 시스템 호출에 해당하는 번호임을 알 수 있다.

```C
/* From lib/user/syscall.c */
/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0)                                           \
        ({                                                               \
          int retval;                                                    \
          asm volatile                                                   \
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" \
               : "=a" (retval)                                           \
               : [number] "i" (NUMBER),                                  \
                 [arg0] "g" (ARG0)                                       \
               : "memory");                                              \
          retval;                                                        \
        })
```
`syscall1()` 매크로의 인자로 넘어간 `file`은 `exec()`으로 실행할 파일의 경로를 
나타내는 문자열을 가리키는 포인터일 것이다. 그렇다면, `syscall1()` 매크로는 과연 
무슨 역할을 할까? `lib/user/syscall.c`에는 `syscall1()` 말고도 `syscall0()`, 
`syscall2()`, `syscall3()` 또한 정의되어 있지만, 이들은 인자의 개수를 제외하고는 
`syscall1()`과 동일하므로 생략하였다.

`syscall1()`은 인자 `ARG0`와 `NUMBER`를 현재 스택에 push하고, 인터럽트 번호 
0x30에 해당하는 인터럽트를 발생시킨 후, esp 레지스터에 8을 더하는 동작을 함을
알 수 있다. 이후 `retval`을 반환하는데, 인라인 어셈블리의 출력 피연산자 목록(
`"=a" (retval)`)을 보면 `retval`에는 eax 레지스터의 값이 저장됨을 알 수 있다. 

여기까지 알아낸 바에 의해 `exec()` 시스템 호출이 발생하였을 때 Pintos에서 
일어나는 일을 정리해 보자면 다음과 같다. 사용자 프로세스에서 `exec()` 호출이
발생하면, `exec()`에서는 다시 `syscall1()` 매크로에 `SYS_EXEC`과 `file` 인자를
전달하며 이를 호출한다. `syscall1()`은 스택에 `SYS_EXEC`과 `file`을 push하고,
0x30 인터럽트를 발생시키며, 인터럽트 핸들러 실행이 종료되면 두 인자를 버리고 이 
시점의 eax 레지스터의 값을 반환한다.

```C
/* From userprog/syscall.c */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
```
0x30 인터럽트의 핸들러는, Pintos 초기화 과정 중 호출되는 `syscall_init()`에 
따르면, `syscall_handler()`이다.

```C
/* From userprog/syscall.c */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
```
`syscall_handler()`는 상술한 바와 같이 시스템 호출이 발생되었을 때 최종적으로
실행되는 함수로, 0x30 인터럽트의 핸들러 역할을 한다. `syscall_handler()`의 
정의를 살펴보면, 현재 구현은 단지 `system call!`이라는 메세지만을 출력하고
스레드를 종료시킨다는 것을 알 수 있다.

아직 분석은 끝나지 않았다. 그렇다면, `syscall_handler()`의 인자인 `f`에는 어떤
정보가 담겨있을까?

```C
/* From threads/interrupt.h */
/* Interrupt stack frame. */
struct intr_frame
  {
    ...

    /* Pushed by the CPU.
       These are the interrupted task's saved registers. */
    void (*eip) (void);         /* Next instruction to execute. */
    uint16_t cs, :16;           /* Code segment for eip. */
    uint32_t eflags;            /* Saved CPU flags. */
    void *esp;                  /* Saved stack pointer. */
    uint16_t ss, :16;           /* Data segment for esp. */
  };
```
<!-- TODO: intr-stubs.S 및 interrupt.c 발췌하여 서술 -->
Project 1 design report에서 서술한 바와 같이, IA32 아키텍처에서 `int` 명령어로
인터럽트가 발생하면, 프로세서는 IDTR이 가리키는 IDT를 참조해 해당하는 ISR을 
호출한다. 또한, 프로세서는 IDT 게이트의 GDT selector를 cs 레지스터에 load한다.
cs 레지스터의 최하위 2비트는 프로세서의 특권 수준(privilege level)을 나타내므로, 
이 과정에서 프로세서의 실행 모드가 사용자 모드에서 OS/hypervisor 모드로 바뀌게 
된다.

Pintos에서는 `threads/interrupt.c`의 `intr_init()`에서 IDTR을 IDT를 
가리키도록 초기화한다. 각 IDT에는 `intr_stubs.S`에 정의된 `intrNN_stub`(`NN`은 
`00`부터 `FF`까지의 16진수)가 등록되어 있어, 인터럽트 발생시 해당 루틴으로 실행 
흐름을 바꾼다. 이후 `intrNN_stub`은 `intr_entry`로 점프하고, `intr_entry`는 
`intr_handler()`를 호출하며, `intr_handler()`는 `intr_register_int()`에 의해
등록된 인터럽트 핸들러를 호출한다. 이 경우에는 `syscall_handler()`가 인터럽트
번호 0x30에 대한 인터럽트 핸들러가 된다.

`f`의 타입인 `struct intr_frame`을 보면, CPU에 의해 인터럽트가 발생한 프로세스의
스택 포인터, 즉 esp 레지스터가 커널 스택 프레임에 저장됨을 알 수 있다. 즉, 
앞서 설명한 `syscall1()`의 인자 `NUMBER`와 `ARG0`는 인터럽트가 발생한 프로세스의
스택의 맨 위에 저장되어 있으므로, 커널에서는 저장된 스택 포인터로 `NUMBER`와 
`ARG0`를 참조할 수 있을 것이다. 좀 더 자세히 설명하자면, `syscall_handler()`에서
`((int *) f->esp)[0]`은 `NUMBER`의 값이 될 것이고, 이를 통해 시스템 호출의
종류를 결정한 후 시스템 호출 인자의 타입에 따라 `((t *) f->esp)[1]`과 같이 
`ARG0`의 값도 참조할 수 있을 것이다.

### Exception Handling Procedure in Pintos
프로그램이 실행되면서 프로그램은 정적으로 예측할 수 없는 다양한 예외적 상황에
놓이게 된다. 예를 들어, 가장 쉽게 발생시킬 수 있는 종류의 예외로는 '0으로 
나누기' 예외가 있으며, 유효하지 않은 명령어 코드(opcode)를 실행하려 할 때에도 
예외가 발생한다. 또한, 페이지 디렉터리 혹은 페이지 테이블에 해당 VPN(Virtual
Page Number)에 해당하는 PDE(Page Directory Entry) 혹은 PTE(Page Table Entry)가
존재하지 않는 경우에도 CPU는 페이지 폴트 예외를 발생시켜 운영체제로 하여금 이를
처리하도록 요구한다.

```C
/* From userprog/exception.c */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  ...
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}
```
이러한 예외는 CPU에 의해 직접 발생하는 경우가 있다는 점을 제외하고는 상술한
인터럽트의 처리 과정과 거의 동일하게 처리되므로 어떻게 예외 발생 시 예외 처리
핸들러가 호출되는지에 대한 설명은 생략한다. `int 0x30`으로 인해 0x30 인터럽트가
발생하여 시스템 호출 핸들러 `syscall_handler()`가 호출되는 것과 같이, 0으로 
나누기 예외가 발생하였을 때에도 CPU는 `intr00_stub`과 `intr_entry`를 통해
결국, 위 코드에 따르면, `kill()`을 호출하게 된다.

한 가지 눈여겨볼 점은 예외에 따라 `intr_register_int()`의 두 번째 인자인 `dpl`이
달라진다는 점이다. `dpl`은 해당 예외를 `int` 명령어로 직접 발생시키기 위해 
필요한 최소한의 특권 수준(privilege level)을 의미한다. IA32 아키텍처에는 4개의 
특권 수준이 있지만 보통은 OS/hypervisor 수준을 의미하는 0과 사용자 수준을 
의미하는 3의 두 가지만 쓰인다. 만약 `intr_register_int()`가 `dpl`을 3으로 하여
호출되었다면 해당 예외는 `int`, `int3`, `int0` 혹은 `bound` 명령어를 통해 사용자
프로그램 스스로 발생시킬 수 있다. 하지만, 만약 `dpl`이 0이라면, 해당 예외는
사용자가 직접 발생시킬 수 없으며 오직 OS/hypervisor 수준에서 발생시키거나 해당
예외 조건이 만족되어야 발생한다.

Pintos에서는 `userprog/exception.c`의 `exception_init()`를 초기화 과정 중 
호출하여 이러한 예외 처리 핸들러를 등록한다. 현재 Pintos의 구현을 살펴보면,
거의 모든 종류의 예외에 대해 `kill()`을 예외 처리 핸들러로 등록하여 예외 발생
시 해당 프로세스를 종료하도록 구현되어 있는 모습을 볼 수 있다.

```C
/* From userprog/exception.c */
/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}
```
`kill()`의 동작을 보면, 인터럽트 프레임에 저장된 cs 레지스터의 값을 확인하여 
예외가 발생한 프로세스의 코드 세그먼트가 사용자 코드 세그먼트(`SEL_UCSEG`)인지,
혹은 커널 코드 세그먼트(`SEL_KCSEG`)인지 확인 후 만약 사용자 코드 세그먼트에서
해당 예외가 발생하였다면 해당 프로세스를 종료한다. 만약 커널 코드 세그먼트에서
해당 예외가 발생하였다면 커널의 버그이므로 커널 패닉을 일으킨다.

페이지 폴트 단 하나의 예외를 제외하면 Pintos의 현재 구현은 모든 예외에 대해
`kill()`이 이를 처리하도록 하고 있다. 페이지 폴트 예외와 페이지 폴트 예외 처리
핸들러에 대해서는 다음 단락에서 더 자세히 살펴볼 것이다.

### IA32 Paging and Address Translation
사용자 메모리 접근과 페이지 폴트 처리에 대해 살펴보기 전에, IA32 아키텍처에서
가상 주소(virtual address)가 실제 물리적 주소로 어떻게 번역되며, Pintos에서는
이러한 번역 과정이 어떻게 처리되는지 알아보자.

IA32 아키텍처에서는 페이지 디렉터리(page directory)와 페이지 
테이블(page table)의 두 계층으로 이루어진 자료구조를 통해 가상 주소를 번역한다.
먼저, 32비트 가상 주소는 다음의 세 비트 필드로 나누어진다.

|       31~22        |      21~12     |    11~0   |
|:------------------:|:--------------:|:---------:|
|Page Directory Index|Page Table Index|Page Offset|

이 중 페이지 디렉터리 인덱스는 페이지 디렉터리를 참조하는데 사용되며, 페이지
디렉터리에 저장된 PDE에는 각각의 페이지 테이블의 주소가 저장된다. 이렇게 PDE를
통해 페이지 테이블을 찾으면, 페이지 테이블 인덱스를 통해 다시 한번 해당 PTE를
참조하며, 이때 PTE에 있는 물리적 주소와 페이지 오프셋을 더한 것이 실제 물리적
주소가 된다. PTE는 하나당 32비트이며, 따라서 20비트의 물리적 주소를 제외한 PTE의
나머지 비트 필드에는 수정 여부(dirty), 접근 여부(accessed), 존재 여부(present),
접근 권한(user/supervisor), 쓰기 가능(writable) 등의 정보가 저장된다. 

그렇다면, 이러한 페이지 디렉터리는 어떻게 프로세서에 의해 참조될까? IA32 
아키텍처 프로세서는 cr0부터 cr8의 컨트롤 레지스터를 가지고 있다. 이 중, cr3
레지스터는 현재 가상 주소 번역에 사용되는 페이지 디렉터리의 주소를 담고 있다.
즉, 가상 주소를 통한 메모리 접근 요청이 들어오면 프로세서는 cr3 레지스터가 
가리키는 페이지 디렉터리에서 (PDE에 담긴) 페이지 테이블의 주소를 찾고, 페이지
테이블에서 다시 해당하는 물리적 주소를 찾아 이를 오프셋과 더해 물리적 주소를
생성한다. 만약 이 과정에서 해당 페이지 테이블 혹은 페이지가 존재하지 않는다면,
즉, 존재 여부를 나타내는 비트(present)가 0이라면 프로세서는 페이지 폴트를 
발생시켜 운영체제로 하여금 이를 처리하도록 할 것이다.

IA32 아키텍처에서는 운영체제에 직접 물리적 주소로 메모리에 접근할 방법을
제공하지 않는다. 따라서, Pintos에서는 `PHYS_BASE` 위의 가상 주소 공간을 
선형적으로 물리적 주소에 대응시키는 방법을 사용한다. 정확히는, 물리적 주소 
`paddr`에 대해 대응되는 커널 영역 가상 주소는 `paddr + PHYS_BASE`가 된다. 
Pintos는 `threads/init.c`의 `paging_init()`을 통해, 해당 영역의 가상 주소가 
물리적 주소에 바로 대응될 수 있도록 페이지 디렉터리와 페이지 테이블을 
초기화한다.

```C
/* From init.c */
static void
paging_init (void)
{
  uint32_t *pd, *pt;
  size_t page;
  extern char _start, _end_kernel_text;

  pd = init_page_dir = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page++)
    {
      uintptr_t paddr = page * PGSIZE;
      char *vaddr = ptov (paddr);
      size_t pde_idx = pd_no (vaddr);
      size_t pte_idx = pt_no (vaddr);
      bool in_kernel_text = &_start <= vaddr && vaddr < &_end_kernel_text;

      if (pd[pde_idx] == 0)
        {
          pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
          pd[pde_idx] = pde_create (pt);
        }

      pt[pte_idx] = pte_create_kernel (vaddr, !in_kernel_text);
    }

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base Address
     of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (init_page_dir)));
}
```
`paging_init()`의 구현을 보면, 먼저 페이지 디렉터리로 사용될 페이지를 
할당받는다. 이후, 물리적 메모리의 각 페이지마다 해당하는 물리 주소를 구하고,
이를 해당하는 가상 주소로 변환한다. 이후, 만약 해당 페이지에 해당하는 PTE가 
저장될 페이지 테이블이 없다면 이를 할당받고, 페이지 테이블을 가리키는 PDE를 
페이지 디렉터리에 등록한다. 이를 커널 영역의 각 메모리 페이지마다 반복하고,
이후 초기화된 페이지 디렉터리를 가리키는 `init_page_dir`을 cr3 레지스터에 
저장하여 이후의 가상 주소 번역이 해당 페이지 디렉터리를 통해 이루어지도록 한다.

```C
/* From threads/vaddr.h */
/* Offset within a page. */
static inline unsigned pg_ofs (const void *va) {
  return (uintptr_t) va & PGMASK;
}

/* Virtual page number. */
static inline uintptr_t pg_no (const void *va) {
  return (uintptr_t) va >> PGBITS;
}

/* Round up to nearest page boundary. */
static inline void *pg_round_up (const void *va) {
  return (void *) (((uintptr_t) va + PGSIZE - 1) & ~PGMASK);
}

/* Round down to nearest page boundary. */
static inline void *pg_round_down (const void *va) {
  return (void *) ((uintptr_t) va & ~PGMASK);
}
```
`threads/vaddr.h`에서는 가상 주소에 대해 페이지 오프셋, 페이지 번호, 페이지 
시작 주소로의 반올림/내림 등 다양한 연산을 할 수 있는 보조 함수를 제공한다. 
또한, 인자 `vaddr`을 `PHYS_BASE`와 비교하여 `vaddr`이 사용자 가상 주소 영역에 
포함되는지 혹은 커널 가상 주소 영역에 포함되는지를 반환하는 `is_user_vaddr()`, 
`is_kernel_vaddr()` (Pintos에서는 `PHYS_BASE` 이상의 가상 주소는 커널 영역,
`PHYS_BASE` 아래의 가상 주소는 사용자 영역으로 간주된다는 점을 기억하자), 물리 
주소 `paddr`을 커널 가상 주소로 변환하는 `ptov()`, 반대로 가상 주소 `vaddr`을
물리 주소로 변환하는 `vtop()` 함수를 제공하며, 이들은 동작이 크게 복잡하지 않은 
보조 함수이기 때문에 보고서에는 포함하지 않았다.

```C
/* From threads/pte.h */
/* Obtains page table index from a virtual address. */
static inline unsigned pt_no (const void *va) {
  return ((uintptr_t) va & PTMASK) >> PTSHIFT;
}

/* Obtains page directory index from a virtual address. */
static inline uintptr_t pd_no (const void *va) {
  return (uintptr_t) va >> PDSHIFT;
}

...

/* Returns a PDE that points to page table PT. */
static inline uint32_t pde_create (uint32_t *pt) {
  ASSERT (pg_ofs (pt) == 0);
  return vtop (pt) | PTE_U | PTE_P | PTE_W;
}

/* Returns a pointer to the page table that page directory entry
   PDE, which must "present", points to. */
static inline uint32_t *pde_get_pt (uint32_t pde) {
  ASSERT (pde & PTE_P);
  return ptov (pde & PTE_ADDR);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable only by ring 0 code (the kernel). */
static inline uint32_t pte_create_kernel (void *page, bool writable) {
  ASSERT (pg_ofs (page) == 0);
  return vtop (page) | PTE_P | (writable ? PTE_W : 0);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable by both user and kernel code. */
static inline uint32_t pte_create_user (void *page, bool writable) {
  return pte_create_kernel (page, writable) | PTE_U;
}

/* Returns a pointer to the page that page table entry PTE points
   to. */
static inline void *pte_get_page (uint32_t pte) {
  return ptov (pte & PTE_ADDR);
}
```

|              31 ~ 12             |11 ~ 9| 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|:--------------------------------:|:----:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Page Table Address or Page Address|  AVL | G |PAT| D | A |PCD|PWT|U/S|R/W| P |

위는 전술한 PDE와 PTE의 비트 필드이다. 

`threads/pte.h`에서는 가상 주소에서 페이지 디렉터리 인덱스, 페이지 테이블 
인덱스를 추출하는 `pd_no()`, `pt_no()`, PDE와 PTE를 위의 비트 필드 형식에
맞춰 생성하는 `pde_create()`, `pte_create_user()`, `pte_create_kernel()` 
함수를 제공한다. 이때, 사용자 영역 페이지를 가리키는 PTE의 접근 권한 비트는
1로 설정되어야 하기 때문에 `pte_create_user()`와 `pte_create_kernel()`이 따로
정의되었다. 마지막으로 PDE에서 페이지 테이블 주소를 추출하는 `pde_get_pt()`와
PTE에서 페이지 주소를 추출하는 `pte_get_page()`가 정의되어 있다.

### Page Directory Abstraction in Pintos
Pintos에서는 하드웨어 수준의 페이지 테이블 조작을 추상화하기 위해 
`userprog/pagedir.c`에 정의된 인터페이스 함수를 제공한다. `pagedir.c`의 
인터페이스 외부에서 프로그래머는 더이상 PTE 혹은 PDE와 직접 상호작용하지 않으며, 
단지 가상 주소에서 물리 주소로의 대응을 담는 '페이지 디렉터리'라는 객체에 대한 
추상적 상호작용만을 이용하게 된다.

```C
/* From userprog/pagedir.c */
/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint32_t *
pagedir_create (void) 
{
  uint32_t *pd = palloc_get_page (0);
  if (pd != NULL)
    memcpy (pd, init_page_dir, PGSIZE);
  return pd;
}
```
`pagedir_create()`는 새로운 페이지 디렉터리를 생성하여 그 페이지 디렉터리의
주소를 반환하는 함수이다. 이때, 새롭게 생성된 페이지 디렉터리 또한 `PHYS_BASE`
위의 커널 영역 가상 주소에 대한 대응은 `init_page_dir`과 같아야 한다. 따라서,
`pagedir_create()`는 먼저 페이지를 할당받은 후 커널 영역에 대해서는 
`init_page_dir`을 복사하는 식으로 동작한다.

```C
/* From userprog/pagedir.c */
/* Destroys page directory PD, freeing all the pages it
   references. */
void
pagedir_destroy (uint32_t *pd) 
{
  uint32_t *pde;

  if (pd == NULL)
    return;

  ASSERT (pd != init_page_dir);
  for (pde = pd; pde < pd + pd_no (PHYS_BASE); pde++)
    if (*pde & PTE_P) 
      {
        uint32_t *pt = pde_get_pt (*pde);
        uint32_t *pte;
        
        for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
          if (*pte & PTE_P) 
            palloc_free_page (pte_get_page (*pte));
        palloc_free_page (pt);
      }
  palloc_free_page (pd);
}
```
위 `pagedir_create()`가 페이지 디렉터리 객체의 생성자라면, `pagedir_destroy()`는
페이지 디렉터리 객체의 소멸자로 기능하는 함수이다. 인자 `pd`의 PDE를 하나씩 
순회하며, 만약 PDE가 현재 존재한다면, 해당 PDE가 가리키는 페이지 테이블에 속한
페이지를 하나씩 free시킨다. 이때 또한, 커널 가상 주소 영역의 페이지는 그대로
유지되어야 하므로 `pd`에서 `pd + pd_no (PHYS_BASE)`, 즉 해당 페이지 디렉터리 중
사용자 영역에 해당하는 페이지만을 free시킨다. 이후 마지막으로 페이지 
디렉터리까지 free시키며 함수 실행은 끝난다.

```C
/* From userprog/pagedir.c */
/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  ...

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}
```
`lookup_page()`는 인자 `vaddr`에 해당하는 PTE를 페이지 디렉터리에서 찾아 이를 
반환하는 함수이다. `lookup_page()`는 먼저 `pd`에서 `vaddr`에 해당하는 PDE를 
찾는다. 이때, 만약 해당 PDE가 `pd`에 존재하지 않는다면, `create` 플래그에 따라
동작이 달라진다. 만약 `create` 플래그가 참이라면 해당 `vaddr`에 대응되는 새로운
페이지 테이블을 만들어 그를 `pd`에 등록하고 해당 PTE를 반환한다. 만약 `create`
플래그가 거짓이라면 새로운 페이지 테이블을 만들지 않고 NULL 포인터를 반환한다.

```C
/* From userprog/pagedir.c */
/* Adds a mapping in page directory PD from user virtual page
   UPAGE to the physical frame identified by kernel virtual
   address KPAGE.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   If WRITABLE is true, the new page is read/write;
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool
pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool writable)
{

  ...

  pte = lookup_page (pd, upage, true);

  if (pte != NULL) 
    {
      ASSERT ((*pte & PTE_P) == 0);
      *pte = pte_create_user (kpage, writable);
      return true;
    }
  else
    return false;
}
```
`pagedir_set_page()`는 `pd`에 `upage`에서 `kpage`로의 대응을 추가하는 함수이다.
만약 `pd`에 `upage`에 해당하는 페이지 테이블이 존재하지 않는 상태에서 새로운 
페이지 테이블을 위한 페이지를 할당받는데 실패하면 거짓을, 해당 대응을 추가하는
데 성공하면 참을 반환한다. 

```C
/* From userprog/pagedir.c */
/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void *
pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}
```
`pagedir_get_page()`는 `pd`에서 `uaddr`에 해당하는 물리 주소를 찾아 반환하는
함수이다. 만약 `uaddr`로부터 어떤 물리 주소에 해당하는 대응이 `pd`에 존재한다면
`uaddr`을 물리 주소로 번역한 결과를 반환하며, 그러한 대응이 존재하지 않는다면
NULL 포인터를 반환한다.

```C
/* From userprog/pagedir.c */
/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void
pagedir_clear_page (uint32_t *pd, void *upage) 
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  pte = lookup_page (pd, upage, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    {
      *pte &= ~PTE_P;
      invalidate_pagedir (pd);
    }
}
```
`pagedir_clear_page()`는 `pd`에서 `upage`를 삭제하는 함수이다. 이때 해당 
페이지는 할당 해제되는것이 아닌, 단지 `pd`에서 `upage`에서 해당 페이지로의 
대응이 삭제되는것임에 주의하자. 따라서, `pagedir_clear_page()`에서는 `upage`에
대응되는 PTE의 존재 여부를 나타내는 비트만 0으로 설정하며, 이후 TLB가 이전의
대응을 참조하여 오작동하지 않도록 `invalidate_pagedir()`을 호출하여 TLB를 
초기화한다. 

```C
/* From userprog/pagedir.c */
/* Loads page directory PD into the CPU's page directory base
   register. */
void
pagedir_activate (uint32_t *pd) 
{
  if (pd == NULL)
    pd = init_page_dir;

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base
     Address of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (pd)) : "memory");
}
```
`pagedir_activate()`는 페이지 디렉터리 `pd`를 활성화하여, 이후 프로세서가 가상
주소를 번역할 때 해당 페이지 디렉터리를 참조하도록 한다. 동작을 자세히 살펴보면,
`pd`를 물리 주소로 번역하여 `movl` 명령어를 통해 해당 물리 주소를 cr3 레지스터에
적재하는 것을 볼 수 있다.

이외의 함수들은 `pagedir.c` 바깥으로 노출되지 않는 정적 함수이거나 동작이 지극히
단순하여 본 보고서에서 설명할 필요가 없으므로 서술을 생략한다.

### User Memory Access and Handling Page Fault
사용자의 시스템 호출을 처리하기 위해 커널은 종종 사용자가 제공한 주소를 
역참조(dereference)하여야 한다. 예를 들어서, 위 단락에서 서술한 바와 같이, 
사용자 프로그램이 시스템 호출을 발생시켰을 때, 커널은 사용자 프로세스의 해당 
시점의 esp 레지스터를 역참조하여 시스템 호출의 종류와 시스템 호출의 인자를 
알아내야 한다. 

문제가 되는 점은 이러한 역참조 시 사용자가 전달한 주소, 혹은 포인터는 신뢰할 수
없다는 점이다. 사용자는 시스템 호출 이전에 esp 레지스터를 조작하여 커널로 하여금
잘못된 값을 읽거나 수정하도록 만들 수 있다. 만약 esp 레지스터가 커널 영역을 
가리킨다면, 즉, esp가 `PHYS_BASE` 위를 가리키고 있다면 사용자 프로세스는 시스템
호출을 통해 본디 불가능해야 할 커널 영역 데이터를 읽거나 수정할 수 있게 된다.

따라서, 운영체제에서는 시스템 호출을 처리하기에 앞서서 사용자 프로세스에서 
전달된 포인터, 레지스터, 혹은 주소 값이 유효한지를 검증하여야 한다. Pintos 
문서에 따르면, 두 가지 접근이 가능하다. 첫 번째 접근법은 사용자 주소를 
역참조하기 이전에 이의 유효성을 검증하는 것이다. 이를 위해서는 먼저
`vaddr.h`의 `is_usesr_vaddr()`을 이용하여 해당 주소가 사용자 영역 가상 주소임을
검증한 후, `pagedir_get_page()`의 반환값을 확인하여 현재 프로세스의 페이지
디렉터리에 해당 주소에 대한 대응이 존재하는지를 확인하면 될 것이다.

하지만, 이러한 방식은 기본적으로 느리다. 그 이유는 다음과 같은데, 현대 
프로세서에는 MMU(Memory Management Unit)라는 주소 번역만을 담당하는 부분이 
존재한다. MMU는 전술한 페이지 디렉터리와 페이지 테이블을 통해 주소를 번역하는
작업을 전담하여 프로세서의 ALU가 실제 연산에 쓰일 수 있도록 돕는다. 첫 번째 
방법은 이러한 MMU를 이용하지 않고 프로세서가 메모리 번역 작업을 담당하게 되므로
느리다.

따라서, 일반적인 운영체제에서 주로 쓰이는 방식은 사용자가 전달한 포인터가
사용자 영역에 있는지만을 검증하고 해당 포인터를 바로 역참조하는 방식이다.
사용자 포인터가 사용자 영역에 있는지를 검증하는 작업은 컴파일러의 최적화에 따라 
하나의 명령어로 완료될 수도 있으므로 경제적이다. 그렇다면, 이 방식을 사용할
경우 사용자가 해당 프로세스의 페이지 디렉터리에 존재하지 않는 페이지에 대한
주소를 전달한다면 커널은 이를 어떻게 처리해야 할까?

프로세서는 현재 cr3 레지스터가 가리키는 페이지 디렉터리에 존재하지 않는 가상
주소를 역참조하고자 하면 페이지 폴트 예외를 발생시킨다. 즉, 두 번째 방식을
채택한 경우에는 페이지 폴트 예외 처리 핸들러에서 해당 상황을 처리해야 한다.
페이지 폴트 예외 처리 핸들러의 등록은 위 단락에서 서술하였다.

```C
/* From userprog/exceptioin.c */
/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
```
페이지 폴트가 발생하면, 프로세서는 cr2 레지스터에 페이지 폴트를 유발한 가상 
주소를 저장하고, 커널 스택에 페이지 폴트의 원인에 따른 오류 코드를 push한 후,
위에서 설명한 예외 처리 과정에 따라 `page_fault()`로 실행 흐름을 옮긴다. 
`page_fault()`의 현재 구현에서는 페이지 폴트의 원인을 나타내는 불리언 변수를
오류 코드에서 추출한 값으로 설정하고 `falut_addr`에 cr2 레지스터에 저장된
가상 주소를 옮기지만, 정작 중요한 페이지 폴트 해결 과정이 구현되어 있지 않고
그저 페이지 폴트를 발생시킨 프로세스를 종료시키기만 한다.

만약 사용자 메모리 영역에 접근하는데 두 번째 방식을 취한다면, `page_fault()`
함수를 수정하여 만약 사용자가 제공한 포인터가 유효하지 않아 페이지 폴트가 
발생한 경우 이를 처리하도록 해야 할 것이다. 두 접근법 모두, 만약 사용자 
프로세스가 유효하지 않은 주소를 전달하였더라도 메모리 누수 등의 자원 낭비 없이
프로세스를 종료시켜야 한다.

### File System of Pintos
이번 프로젝트에서 구현해야 하는 몇몇 시스템 호출은 파일에 수정하거나 생성하고
제거하며 여는 등의, 파일 시스템과 상호작용하는 작업을 포함한다. 따라서, 이들을
구현하기에 앞서서 Pintos에서 제공하는 파일 시스템 인터페이스를 살펴보자.

Pintos에서 파일, 다시 말해 `struct file`은 `inode`와 파일의 현재 위치를 나타내는
`pos`으로 이루어져 있다. `pos`는 단지 파일에 읽거나 쓸 위치를 나타내므로,
실제 파일에 대한 작업은 `inode`를 통해 이루어진다고 볼 수 있다. 그렇다면, 
`inode`는 어떤 구조체, 혹은 자료구조일까?

```C
/* From filesys/inode.c */
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };

...

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
```
`filesys/inode.c`의 정의를 보면 `struct inode`는 현재 블록에서 섹터 위치를
나타내는 `sector`와 실제 디스크 상에서 `inode`를 나타내는 `inode_disk` 형 
`data`의 두 원소와 함께 여러 메타데이터를 가지고 있음을 알 수 있다. 또한,
`struct inode_disk`는 다시 디스크 상에서 처음 섹터를 나타내는 `start`와
해당 `inode_disk`의 크기를 나타내는 `length`, 그리고 `inode_disk`의 크기를
한 섹터 (512B) 크기로 맞추기 위해 삽입된 `unused`의 멤버가 있음을 알 수 있다.

이때 블록, 혹은 `struct block`은 디스크와 같이 연속된 저장 공간을 가진 장치의
추상화이다. 따라서, Pintos에서는 블록에 대한 연산도 `devices/block.h`와 
`devices/block.c`에서 제공하지만, 해당 모듈에 대한 설명은 이 프로젝트의 범위를 
벗어나므로 생략한다. 

따라서, `inode_disk`는 디스크의 특정 섹터, `start`에서 시작해 `length`만큼 
이어지는 어떤 자료를 나타내는 자료구조이다. 이때, `length`가 한 섹터의 
크기보다 크다면 해당 자료는 여러 섹터에 나누어져 저장될 수도 있을 것이다.
`inode`는 이러한 `inode_disk`에 더해, 현재 읽거나 쓰고 있는 섹터를 저장하는 
자료구조이다. 

Pintos는 이러한 추상화를 통해 메모리 상에 저장되어 있는 한 묶음의 데이터를 
표현한다. (의도적으로 파일이라는 말을 배제하고 설명하는 중이다.) 그렇다면, 
이러한 `inode`에 대한 공개 인터페이스 함수에는 무엇이 있을까?

```C
/* From filesys/inode.c */
/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}
```
`inode` 모듈을 초기화하는 함수이다. `open_inode`는 현재 열려있는 `inode`들을
저장하는 전역 리스트이다.

```C
/* Frome filesys/inode.c */
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}
```
`length`만큼의 길이를 가지는 `inode`를 초기화하고, 이를 `sector`가 가리키는
섹터에 쓰는 명령어이다. `free_map_allocate()`는 `sectors` 만큼의 블록 섹터를
할당받는 함수이다. 이렇게 디스크 공간을 할당받은 후, `block_write()` 함수를
호출하여 `sector`가 가리키는 섹터에 해당 `disk_inode`를 쓴다. 이후 할당받은
섹터를 모두 0으로 초기화하고, 블록에 이미 저장되어 쓸모없어진 `disk_inode`를
할당 해제하며 함수 실행은 끝난다.

```C
/* Frome filesys/inode.c */
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}
```
`sector`에 해당하는 `disk_inode`를 열어 그 결과를 `inode`로 반환하는 함수이다. 
만약 해당 `sector`에 해당하는 `disk_inode`가 이미 열려 있는 상태라면 
`inode_reopen()`을 호출하여 새로운 `inode`를 위한 공간을 할당받는 과정을
생략한다. 만약 해당 `inode`가 현재 열려있지 않다면 새로운 메모리를 할당받고,
`open_inodes`에 새로운 `inode`를 삽입하며, `inode` 정보를 초기화하고 블록에서
해당 `inode`에 해당하는 `disk_inode`를 읽어 이를 `inode->data`에 저장한다.

```C
/* Frome filesys/inode.c */
/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}
```
만약 현재 열고자 하는 `inode`가 이미 열려있는 경우 해당 `inode`의 `open_cnt`만을
증가시키는 함수이다.

```C
/* Frome filesys/inode.c */
/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}
```
해당 `inode`의 inode 번호를 반환하는 함수이다.

```C
/* Frome filesys/inode.c */
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}
```
`inode`를 닫는 함수이다. 만약 닫은 이후에도 `inode`의 `open_cnt`가 0이 아니라면,
해당 `inode`를 누군가 계속 열고 있다는 뜻이므로 해당 `inode`를 할당 해제하지는
않는다. `free_map_release`는 `inode_open()`에서 할당받았던 블록상의 공간을 다시 
할당 해제하는 함수이다. 이때, 만약 해당 `inode`를 열고 있는 다른 스레드가 
존재하지 않는다면, `inode`를 저장하는 메모리 공간을 할당 해제한다. 이에 더해, 
만약 해당 `inode`가 제거된 상태라면 `dick_inode`를 저장하던 섹터와 실제 데이터를
저장하던 섹터들 모두를 할당 해제하여 다른 `disk_inode`에서 해당 공간을 사용할 수
있게 한다.

```C
/* Frome filesys/inode.c */
/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}
```
`inode`를 삭제됨 상태로 바꿔 이후 `inode`가 닫힐 때 해당 블록 공간을 할당 해제할
수 있도록 하는 함수이다.

```C
/* Frome filesys/inode.c */
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}
```
`inode`를 `offset`에서부터 시작하여 `size`만큼 읽어들여 `buffer_`에 저장하는
함수이다. 이 함수는 먼저 각 섹터에서 읽어들어야 할 `chunk_size`를 계산한다.
만약 `chunk_size`가 한 섹터의 크기와 같다면 해당 섹터를 통째로 `buffer`로 
복사한다. 만약 `chunk_size`가 한 섹터의 크기보다 작다면, 먼저 `bounce`라는
`buffer`의 버퍼에 섹터를 복사한 후 `bounce`에서 `chunk_size`만큼을 `buffer`에
복사한다. 이후 읽어들인 크기만큼을 남은 읽어들일 크기인 `size`에서 빼고,
다음 읽을 위치인 `offset`, 다음 `buffer`에 저장할 위치인 `bytes_read`에 읽어들인
크기만큼을 더한다.

```C
/* Frome filesys/inode.c */
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}
```
`inode`가 가리키는 섹터의 `offset` 위치에 `size`만큼을 `buffer`로 쓰는 함수이다.
동작은 `block_read()`가 `block_write()`로 바뀐 것을 제외하고는 
`inode_read_at()`과 같으나, 특정 섹터 전부에 쓰는 것이 아니라면 일단 해당
섹터를 읽어서 수정될 부분만을 써야 하기 때문에 `block_read()`를 호출해 
일단 해당 섹터를 읽고, `bounce`에 쓰기를 수행한 이후, 해당 데이터를 블록에
쓴다는 차이점이 있다.

```C
/* Frome filesys/inode.c */
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
```
해당 `inode`에 쓰기를 금지/허용하는 함수와 `inode`의 길이를 반환하는 함수이다.

```C
/* From filesys/file.c */
/* An open file. */
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
```
Pintos에서 파일은 상술한 `inode`와 파일에 읽거나 쓸 위치를 저장하는 `pos`, 
그리고 파일 쓰기를 거절 혹은 허용할지를 나타내는 `deny_write`의 멤버를 가지고
있다. 파일에 대한 인터페이스 함수들은 대부분 `inode`에 대한 인터페이스로 
구현되어 있다. 즉, Pintos에서 파일은 `inode`에 현재 읽거나 쓸 위치에 대한 개념이
더해진 자료구조라 볼 수 있다.

```C
/* From filesys/file.c */
/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}
```
`inode`에 해당하는 파일을 여는 함수이다. 만약 인자로 넘어온 `inode`가 NULL
포인터이거나 파일 구조체를 저장할 메모리 할당에 실패하면 NULL 포인터를 반환한다.
이후 파일 인터페이스 함수들은 단순한 `inode`에 대한 감싸기(wrapper) 함수이므로
설명을 생략한다.

Pintos 파일 시스템 추상화 계층의 최상위에는 `filesys` 모듈이 있다. `filesys`
모듈은 블록, `inode`, 파일까지 모든 것을 총망라하여 프로그래머에게 파일 처리
기능을 제공한다.

```C
/* From filesys/filesys.c */
/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}
```
파일 시스템을 초기화하는 함수이다. 먼저 블록 장치를 `block_get_role`을 통해 
가져온 후, 상술한 `inode`를 초기화하는 `inode_init()`, 현재 파일 시스템에
비어있는 섹터의 위치를 저장하는 자료구조를 초기화하는 `free_map_init()`을
호출하고, 만약 `format` 인자가 참이라면 파일 시스템을 포맷한다. 마지막으로
블록 장치에 파일로 저장된 `free_map`을 `free_map_open()`을 통해 열면서 함수
실행은 끝난다.

```C
/* From filesys/filesys.c */
/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}
```
파일 시스템 사용이 끝난 후 이를 종료하는 함수이다.

```C
/* From filesys/filesys.c */
/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}
```
`name`에 해당하는 파일을 여는 함수이다. 루트 디렉터리에서 파일 이름에 해당하는
`inode`를 `dir_lookup()` 함수를 이용해 찾고, 이후 `file_open()` 함수를 호출하여
해당 파일의 파일 포인터를 반환한다.

```C
/* From filesys/filesys.c */
/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}
```
파일 시스템에서 `name`이라는 이름을 가진 파일을 삭제하는 함수이다. 이때, 열려
있는 파일을 삭제하더라도 이미 열려있는 파일에는 영향을 주지 않고 이후에 해당
파일에 대한 접근만을 막도록 구현되어있다.

주의해야 할 점은 현재 Pintos 구현의 파일 시스템은 내부적으로 데이터 일관성이
보장되지 않고, 파일의 크기를 바꿀 수 없으며, 파일 하나는 언제나 블록 장치 내의 
연속된 공간을 차지하도록 구현되어 있는 등의 다양한 제약조건이 있다는 점이다.
특히 파일 시스템 내부적으로 동기화가 구현되어있지 않다는 점은 이후 파일 시스템
인터페이스를 이용해 시스템 호출을 구현할 때 주의하여야 할 점이다.

## Design Description

### Process Termination Messages
Exit 의 실행이나 혹은 다른 이유로 인해 user process가 종료될 때에 다음과 같이 process의 이름과 exit code를 출력하는 함수를 구현하고자 한다. 출력되는 이름은 process_execute() 에 입력된 name과 같은 이름이며, 만약 user process가 아닌 Kernel process가 종료되었을 경우에는 출력하지 않도록 유의해야 한다.
```C
printf ("%s: exit(%d)\n", ...);
```

구현 방법의 경우 system call 형식을 사용하여 syscall.c 에 syscall_exit()과 같이 exit system call handler를 선언하고 handler 내부에서 위의 형식에 맞춘 Printf() 실행 이후 thread_current() 를 통해 현재 thread를 thread_exit() 을 실행하는 함수를 선언하여 사용할 수 있을 것이다. 또한, 해당 syscall_exit()을 바탕으로 오류가 발생한 경우에도 활용하여 User process 종료 시 termination message를 출력할 수 있게 된다.

### Argument passing
현재 구현은 process_execute() 가 실행 시에 새롭게 생성되는 process에게 argument passing 이 구현되지 않은 상황이다. 이말은 즉, process_execute() 함수의 실행 과정 내에서 입력받은 command line을 공백에 따라 구분하여 새롭게 생성될 Process 에게 전달해주는 것이 필요하다. 이 때 공식 문서에서 설명하고 있는 80x86 을 통해 실행이 되어야 한다. 

우선 C 언어 기반의 user program 이 실행 될 때 pintos 에서는 user/entry.c 내에 위치한 _start() 함수를 통해 main() 함수가 실행되게 된다. 이 때 main()을 exit() 함수가 감싸고 있기 때문에 _start() 내에서 main() 을 실행하고 실행이 종료되기 까지를 기다리게 되는 방식으로 구현되어 있다. 이 때 main() 함수를 실행할 때 넘겨주게 되는 agrument는 개수를 저장하는 argc와 실제 argument인 argv로 구성되어 있다. 이 argv를 올바르게 넘겨주기 위해서는 80x86의 calling convention을 따라야 한다. stack 을 구성할 때 주요한 특성은 argument를 오른쪽에서 왼쪽 순서대로 push하고 4의 배수로 stack pointer가 위치하도록 aligment를 해줘야 하며, argv를 push 한 이후에 차례대로 argc와 c convention을 맞춰주기 위한 return address를 추가해줘야 한다. 하지만 이 때 _start() 는 실제로 return이 의미가 없으므로 void address인 0을 Push 해주게 된다. 또한, Command line을 공백에 따라 parsing 하기 위해서 구현된 함수는 strtok_r 함수로 공백을 기준으로 문자열을 parsing 할 수 있다.

자세한 calling convetion은 다음과 같다. caller가 argument를 stack에 오른쪽에서 왼쪽 순서대로 Push 하고, 이때 stack 의 pointer 는 줄어드는 방식으로 push 된다. 이후 Caller는 return address를 Push하게 되면서, callee의 첫 번째 Instruction 으로 jump하게 된다. 이후 callee가 실행되는데 이 때 stack pointer가 return address, 첫 번째 argument, 두 번째 Argument 순서대로 가르키고 있게 된다. 이후 callee가 실행을 마무리하고 return value가 존재한다면 EAX에 저장하고 return address 를 pop 하여 해당 주소로 Jump 하게 된다. (Ret 명령어 사용) 마지막으로 stack 에 남아있는 모든 argument를 pop 하게 된다.  

구현할 부분은 다음과 같이 정리할 수 있을 것이다. process_execute() 함수 내에서 입력받은 command line을 strtok_r 함수를 통해, 실행하고자 하는 파일의 이름과 나머지를 구별하게 된다. 이후 start_process() 함수 내에서 함수의 이름을 통해 load() 를 실행하고 나머지 인자도 마찬가지로 strtok_r() 을 통해서 인자들을 공백에 따라 구별하게 되며, 이 함수 내에서 위에서 설명한 calling convention에 맞춰서 stack 을 구성하여 이후 해당 user program 을 실행할 환경을 구성해주게 될 수 있을 것이다. 따라서 strtok_r()를 사용하여 process_execute() 에서 parsing 하는 부분과, start_process() 내에서 stack 에 push 하여 convention에 맞도록 argv를 구성하는 부분을 구현해야 할 것이다.

### System Calls
Pintos에서 사용자 프로세스가 시스템 호출을 발생시켰을 경우 상술한 대로 0x30
인터럽트를 발생시킨 후 `syscall_handler()`로 실행 흐름을 넘겨 해당 함수에서
시스템 호출을 처리하게 된다. 따라서, 각각의 시스템 호출에 대한 처리 방법을
고민하기에 앞서서 어떻게 시스템 호출의 종류를 알아내고 시스템 호출과 같이
넘겨진 인자를 처리할 것인지를 고민해야 한다.

상술한 바와 같이, 이는 `syscall_handler()`의 인자 `f`의 멤버 `esp`를 통해 접근
가능하다. 사용자 프로세스는 시스템 호출을 발생시키며 시스템 호출의 각 인자들과
시스템 호출 번호를 스택에 push하며, 따라서 `f->esp`를 역참조한 결과는 시스템
호출 번호가 될 것이다. 또한, `f->esp`를 역참조하기 전 `f->esp`가 유효한 사용자 
영역 가상 주소인지를 확인하여야 한다.

시스템 호출 번호를 통해 시스템 호출의 종류를 알아냈다면, 이제는 시스템 호출의
인자 목록을 알아낼 차례다. 전술한 바와 같이, 사용자 스택의 시스템 호출 번호 
아래에는 0개 혹은 그 이상의 인자들이 push되어 있다. 인자의 개수는 시스템 호출
종류를 통해 알 수 있다.

이후에는 시스템 호출의 종류에 따라 해당하는 처리 루틴을 실행하면 될 것이다. 
각 시스템 호출 전부를 `syscall_handler()`에서 처리하는 것은 코드의 가독성에 
좋지 못한 선택이므로, `static pit_t exec (void *esp)`와 같이 각 시스템 호출을 
처리하는 함수를 따로 정의하여 그 함수에서 각각의 시스템 호출을 처리하도록
하는 것이 좋을 것이다.

x86 호출 규약에 따르면 함수의 반환값은 eax 레지스터를 통해 전달되어야 한다.
따라서, `f->eax`를 각 시스템 호출의 처리 결과로 바꾸어 사용자 프로세스에게 
시스템 호출이 마치 일반적인 함수 호출과 같이 실행되는 것처럼 보이도록 하여야 
한다.

또한 마지막으로 사용자 영역 가상 주소를 역참조하는 도중 페이지 폴트가 
발생하였을 때, `page_fault()`에서 이를 적절히 처리할 수 있도록 `page_fault()`의
코드를 변경하여야 할 것이다.

위의 논의사항을 (완성되지 않은) 코드로 표현하면 다음과 같을 것이다.
```C
/* From userprog/syscall.c */
void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;
  uint32_t retval;

  if (is_user_vaddr (f->esp))
    syscall_number = ((int *) f->esp)[0];
  else
    /* The user stack pointer is faulty. Make user process that invoked system 
       call exit. Be sure not to leak resources when killing the process. */

  switch (syscall_number) {
    case SYS_HALT:
      halt (void);
      break;
    ...
    case SYS_TELL:
      retval = (uint32_t) tell (f->esp);
      break;
    ...
  }

  f->eax = retval;
}
```
```C
/* From userprog/exception.c */
static void
page_fault (struct intr_frame *f) 
{
  ...

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  if (not_present && !write && !user)
    /* This page fault is raised while handling system call. */

  ...
}
```

#### User-Process-Manipulating System Calls
이번 프로젝트에서 구현해야 할 시스템 호출의 종류를 크게 나누자면 다른 사용자
프로세스에 영향을 미치는 시스템 호출과, 파일 시스템을 수정하는 시스템 호출로
나눌 수 있을 것이다. 전자에는 `halt()`, `exit()`, `exec()`, `wait()`가 포함되며,
나머지 시스템 호출은 모두 파일 시스템과 관련된 시스템 호출이다.

`halt()` 시스템 호출의 경우 Pintos 문서에서 서술된 바와 같이 
`shutdown_power_off()` 호출로 시스템을 종료하면 될 것이다.

`exit()`, `wait()`의 경우 조금 복잡한데, 어떤 프로세스가 종료하는 경우 그 
프로세스를 기다리고 있는 모든 프로세스를 깨우고, `wait()`의 반환값으로 
기다리고 있던 프로세스의 종료 코드를 전달해야 하기 때문이다. 한 가지 가능한 
구현으로는 각 스레드에 해당 스레드를 기다리고 있는 스레드의 리스트를 저장하고, 
만약 해당 스레드가 종료될 경우 대기 스레드 목록에 있는 모든 스레드에 종료 코드를
전달하는 방법이 있을 것이다. Pintos의 프로세스는 단 하나의 스레드만을 가질 수 
있기 때문에, '프로세스'와 '스레드'는 맥락에 상관없이 서로 바뀌어 사용될 수 
있다는 점을 기억하자.

`exec()`은 `thread_create()`를 통해 `process.c`의 `load()`를 실행하고, 
`cmd_line`을 쪼갠 인자 목록을 `aux`로 전달받는 새로운 스레드를 생성하면 될 
것이다.

#### File-System-Manipulating System Calls
파일 시스템을 조작하는 시스템 호출의 경우, Pintos에서 제공하는 파일 시스템
인터페이스 함수를 이용하면 될 것이다. 대부분의 시스템 호출이 파일 시스템
인터페이스와 일대일로 대응되며, Pintos의 파일 시스템은 내부적으로 동기화를
보장하지 않아 사용자가 이를 사용할 때 명시적으로 임계 구역에 대한 lock 등의
동기화 대책을 강구해야 한다는 점을 주의하며 구현하면 될 것이다.₩:ㅈ

### Denying Write to Executables
process가 실행 중인 경우 해당 file을 수정하면 큰 문제가 발생할 가능성이 존재한다. 따라서 process를 실행하기 전에 write을 금지하는 부분이 추가적으로 필요하게 된다. 또한, 만약 write을 금지했다면 process를 종료하기 이전에 다시 allow 해주는 과정이 필요한데 이 때 file 을 기록하고 있어야 하므로, struct thread에 추가적으로 현재 실행되고 있는 file을 기록해주는 것이 필요할 것이다. 위의 금지하는 부분은 Process.c의 Load() 함수 내에서 해당 파일 이름을 current thread에 저장해주고, 이후에 file_deny_write() 을 통해서 실행 중에 파일에 쓰기가 되는 것을 방지할 수 있다. 또한, process_exit() 함수 내에서 종료하기 이전에 file_allow_write() 함수를 통해서 current_thread에 기록된 file을 바탕으로 다시 allow 해주고 종료하는 것을 통해 구현할 수 있게 될 것이다.

## Design Discussion
이번 내용은 boot strap부터 포함하여 file system, thread, process 등 전반적인 이해가 필요하다 보니 굉장히 이해하는 데 많은 시간이 소요되었다. 특히 user program 을 실행하기 위해서 생각했던 것은 VM 과 관련된 부분 정도만 고려하고 있었는데, File system 적인 측면에서 Inode로 관리되는 방법론에 대해서 처음 이해하게 되었으며, pintos에서 어떻게 file system 을 manage 하는지에 대한 과정을 보다 명확하게 이해할 수 있어서 큰 도움이 된 것 같다. 또한, VM을 관리하는 과정에 있어서도 TSS와 같은 여러 struct를 통해 Context switch 시에도 정보를 유지하는 과정에 대해서도 보다 명확하게 이해할 수 있었다. 전반적인 pintos에 대해서 다시 알아간 느낌이지만, 구현시에 syscall 이 약 400줄 이상이 예정되어 있으며, filesystem 을 이용할 때 lock을 통해서 구현해야 한다는 점에서 굉장히 난이도가 있을 것으로 생각된다.