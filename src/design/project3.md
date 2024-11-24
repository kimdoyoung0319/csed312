# Project 3 - Design Report (Team 28)

## Frame Table
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->

## Lazy Loading
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

## Supplemental Page Table
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

## Stack Growth
### Basic Descriptions & Limitations
 기존 pintos에서는 stack 이 single page(4 KB)로 제한되어 있었다. 
즉, single page를 넘어가는 stack space가 필요한 경우에는 page fault가 
발생하여 이를 handling은 하지만, stack 자체를 확장하는 것이 아니라 
단순히 오류를 처리하는 것에 가까웠다. 결국 이는 큰 용량을 가지는 프로그램이 
실행할 수 없게 되는 문제를 야기하게 된다. 따라서 이를 해결하고자 한다.

### Necessity
 기존 구현을 간단히 살펴보면 ```userprog/exception.c``` 에 위치한 
```static void page_fault (struct intr_frame *f) ``` 
을 통해서 구현되어 있다. 해당 handler의 실행 과정 중에서 fault가 발생한
address에 따라서 stack growth 가 필요한 경우 실행할 수 있도록 구현을 수정해야 한다.
 우선 필요한 것은 입력받은 인터럽트 프레임 내에서 해당 주소가 어떤 상황인지 확인해야 한다.
fault_addr을 확인하여 stack growth 가 필요한지 여부를 알아야 한다. 공식 문서에서 
확인할 수 있는 것처럼 x86 에서 기본적으로 esp 아래의 주소를 참조하는 경우는 PUSH 혹은
PUSHA 인스트럭션이 존재하는데 각각 4 btye, 32 byte 아래의 주소를 참조하게 된다. 
따라서 최대 32 byte 이내의 주소를 참조했을 때 stack growth를 통해 지원해야 하며, 
나머지 경우에 대해서는 해당 명령을 오류로 처리하여, 프로세스를 종료해야 한다. 또한, 
마찬가지로 공식 문서에서 추천하는 것처럼 8 MB (PHYS_BASE로 부터)를 최대 stack size로 
제한하여, 이 영역을 초과하게 된다면 heap 영역까지 초과할 수 있으므로 마찬가지로 8 MB를 
초과한 경우에도 프로세스를 종료해야 한다. 마지막으로 확인할 부분은 당연하게도 할당되었는지 
여부를 확인해야 한다. 즉, supplemental Page Table을 확인하여 fault_addr에 
해당하는 page가 이미 로드가 되어있는지 여부를 확인하여, 해당 page가 존재할 경우에는 
확장을 따로 수행하지 않는 대신에, write이 가능한 경우에는 해당 page를 다시 
memeory로 불러와야 한다. 또한, 마지막으로 필요한 것은 user mode가 아닌 
kernel mode 에서 실행되었을 경우에는 f->esp가 kernel 의 stack 이므로 stack
growth를 구현하기 위해서는 user thread의 esp를 따로 저장하고, 그 stack을 
키워주는 부분을 고려해야 한다.

### Design Proposal
 위에서 언급한 바와 같이 exception.c 의 page_fault() 를 수정하는 방식으로 구현한다.
기존 코드에 존재하는 not_present, write, user를 바탕으로 not_present 시에는 
fault_addr을 확인하여 Supplemental Page Table을 직접 확인하여 load 되었는지 
여부를 확인하고 아니라면 해당 주소가 32 bytes 이내인지 여부, PHYS_BASE 로부터 8MB 
이내인지 여부를 확인해야 한다. 만약 전부 해당한다면, user mode 인 경우에는 f->esp를 
키워주고, kernel mode인 경우에는 기존에 실행되고 있던 user thread에 저장된 stack
pointer를 저장하여 해당 esp를 growth 해주는 방식으로 구현하고자 한다. 이 때 growth
시에는 pg_round_down (fault_addr) 에 해당하는 주소에 새로운 page를 할당하는 
방식으로 구현하고자 한다.

## File Memory Mapping
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

## Swap Table
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->

## On Process Termination
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->
