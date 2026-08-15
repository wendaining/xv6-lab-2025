我需要完成这个 Lab，然后下面的 Lab 的内容介绍。你需要帮助我的是：引导我完成这个 lab，在我没说允许写代码的前提下绝对不写代码，大部分以通俗易懂的引导为主。

# Lab：Networking（网络）

在这个 Lab 中，你将为网络接口卡（NIC）编写一个 xv6 设备驱动，然后实现 Ethernet/IP/UDP 协议处理栈的接收部分。

获取这个 Lab 的 xv6 源码，并切换到 `net` 分支：

```bash
$ git fetch
$ git checkout net
$ make clean
```

## 背景

在开始写代码之前，你可能会发现复习 [xv6 book](https://pdos.csail.mit.edu/6.828/2025/xv6/book-riscv-rev5.pdf) 的 “Chapter 6: Interrupts and device drivers（中断与设备驱动）” 很有帮助。

你将使用一种叫作 E1000 的网络设备来处理网络通信。对于 xv6（以及你要编写的驱动）来说，E1000 看起来就像是一块连接到真实 Ethernet 局域网（LAN）上的真实硬件。实际上，你的驱动要与之通信的 E1000 是由 QEMU 模拟出来的，而它连接的 LAN 也同样由 QEMU 模拟。

在这个模拟 LAN 上：

- xv6（“guest”，客户机）的 IP 地址是 `10.0.2.15`。
- QEMU 会让运行 QEMU 的计算机（“host”，宿主机）在这个 LAN 上表现为 IP 地址 `10.0.2.2`。

当 xv6 使用 E1000 向 `10.0.2.2` 发送一个 packet 时，QEMU 会把这个 packet 交给宿主机上的相应应用程序。

你将使用 QEMU 的 “user-mode network stack（用户模式网络栈）”。QEMU 的文档中有更多关于 user-mode stack 的说明，可见[这里](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29)。我们已经更新了 Makefile，使其启用 QEMU 的 user-mode network stack 和 E1000 网卡模拟。

Makefile 会配置 QEMU，把所有进入和离开的 packet 都记录到 Lab 目录中的 `packets.pcap` 文件里。查看这些记录可能有助于确认 xv6 是否发送和接收了你预期的数据包。可以使用下面的命令显示这些记录：

```bash
tcpdump -XXnr packets.pcap
```

这个 Lab 已经向 xv6 仓库中加入了一些文件。

`kernel/e1000.c` 包含 E1000 的初始化代码，以及用于发送和接收 packet 的空函数，你需要补全它们。

`kernel/e1000_dev.h` 包含 E1000 的寄存器和 flag 位定义，这些定义来自 Intel E1000 的 [Software Developer's Manual](https://pdos.csail.mit.edu/6.828/2025/readings/8254x_GBe_SDM.pdf)。

`kernel/net.c` 和 `kernel/net.h` 包含一个简单的网络栈，实现了 [IP](https://en.wikipedia.org/wiki/Internet_Protocol)、[UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) 和 [ARP](https://en.wikipedia.org/wiki/Address_Resolution_Protocol) 协议。`net.c` 已经包含用户进程发送 UDP packet 的完整代码，但缺少接收 packet 并把它们交给用户空间所需的大部分代码。

最后，`kernel/pci.c` 包含 xv6 启动时在 PCI 总线上寻找 E1000 网卡的代码。

## Part One：NIC（[中等难度](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html)）

你的任务是完成 `kernel/e1000.c` 中的 `e1000_transmit()` 和 `e1000_recv()`，使驱动能够发送和接收 packet。

当 `make grade` 显示你的实现通过 `"txone"` 和 `"rxone"` 测试时，这一部分就完成了。

写代码时，你需要参考 E1000 的 [Software Developer's Manual](https://pdos.csail.mit.edu/6.828/2025/readings/8254x_GBe_SDM.pdf)，尤其是：

- Section 3.2 描述 packet reception（数据包接收），但可以跳过 3.2.8 和 3.2.9。
- Sections 3.3.1、3.3.2、3.3.3 和 3.4 描述 transmission（数据包发送）。
- Section 13 描述 E1000 的寄存器，需要时作为参考即可；不要把整章都读完。

这份 [Software Developer's Manual](https://pdos.csail.mit.edu/6.828/2025/readings/8254x_GBe_SDM.pdf) 描述了几种关系密切的 Ethernet controller。QEMU 模拟的是 `82540EM`。

你需要熟悉上面提到的 Chapter 3 中的相关部分。其他章节主要讲 E1000 中你的驱动不需要交互的部分。

一开始不用担心各种细节；先大致了解这份文档是怎么组织的，以便以后知道去哪里查即可。

E1000 有很多高级功能，其中绝大多数都可以忽略。完成这个 Lab 只需要一小部分基本功能。

`e1000.c` 中提供的 `e1000_init()` 会配置 E1000：

- 从 RAM 中读取待发送的 packet；
- 把收到的 packet 写入 RAM。

这种技术叫作 DMA（Direct Memory Access，直接内存访问），意思是 E1000 硬件可以直接从 RAM 读取 packet，也可以直接把 packet 写入 RAM。

由于一阵突发到来的 packet 可能比驱动处理它们的速度更快，`e1000_init()` 会给 E1000 提供多个 buffer，让 E1000 可以把收到的 packet 写入其中。

E1000 要求这些 buffer 由 RAM 中的一组 “descriptor（描述符）” 来描述；每个 descriptor 都包含一个 RAM 地址，E1000 可以把收到的 packet 写到这个地址中。

`struct rx_desc` 描述了 descriptor 的格式。

这组 descriptor 构成的数组叫作 receive ring，或者 receive queue。

它之所以是一个环形结构，是因为当网卡或驱动到达数组末尾时，会重新回到数组开头。

`e1000_init()` 会用 `kalloc()` 为 E1000 分配 packet buffer，供它 DMA 写入。

还有一个 transmit ring，驱动应该把想让 E1000 发送的 packet 放进这个 ring 中。

`e1000_init()` 会把两个 ring 的大小分别配置为 `RX_RING_SIZE` 和 `TX_RING_SIZE`。

当 `net.c` 中的网络栈需要发送一个 packet 时，它会调用 `e1000_transmit()`，并传入一个指向待发送 packet 所在 buffer 的指针；`net.c` 会使用 `kalloc()` 分配这个 buffer。

你的 transmit 代码必须把 packet 数据的指针放入 TX（transmit）ring 的一个 descriptor 中。

`struct tx_desc` 描述了这个 descriptor 的格式。

你需要保证每个 buffer 最终都会传给 `kfree()`，但是**只能在 E1000 已经完成该 packet 的发送之后**这么做。

E1000 会在 descriptor 中设置 `E1000_TXD_STAT_DD` 位，表示它已经完成发送。

当 E1000 从 Ethernet 收到一个 packet 时，它会通过 DMA，把 packet 写入下一个 RX（receive）ring descriptor 的 `addr` 所指向的内存。

如果当前还没有一个 E1000 interrupt 等待处理，E1000 会请求 PLIC 在 interrupt 被启用后尽快递送一个 interrupt。

你的 `e1000_recv()` 必须扫描 RX ring，并通过调用 `net_rx()`，把每个新 packet 交给 `net.c` 中的网络栈。

然后你还需要分配一个新的 buffer，并把它放入这个 descriptor 中。这样当 E1000 之后再次走到 RX ring 的这个位置时，就能找到一个新的 buffer，用来 DMA 写入新的 packet。

除了读写 RAM 中的 descriptor ring 之外，你的驱动还需要通过 E1000 的 memory-mapped control registers（内存映射控制寄存器）与它交互：

- 检测是否有已经收到的 packet；
- 告诉 E1000，驱动已经把一些待发送的 packet 填进了 TX descriptor。

全局变量 `regs` 保存了一个指向 E1000 第一个控制寄存器的指针；驱动可以把 `regs` 当作数组，通过下标访问其他寄存器。

尤其需要使用 `E1000_RDT` 和 `E1000_TDT` 这两个下标。

要测试 `e1000_transmit()` 是否能发送单个 packet，可以：

在一个终端窗口中运行：

```bash
python3 nettest.py txone
```

在另一个窗口中运行：

```bash
make qemu
```

然后在 xv6 中运行：

```bash
nettest txone
```

它会发送一个 packet。

如果一切正常，`nettest.py` 会打印：

```text
txone: OK
```

这表示 QEMU 的 E1000 模拟器在 DMA ring 上看到了这个 packet，并把它转发到了 QEMU 外部。

如果发送正常：

```bash
tcpdump -XXnr packets.pcap
```

应该产生类似这样的输出：

```text
reading from file packets.pcap, link-type EN10MB (Ethernet)
21:27:31.688123 IP 10.0.2.15.2000 > 10.0.2.2.25603: UDP, length 5
        0x0000:  5255 0a00 0202 5254 0012 3456 0800 4500  RU....RT..4V..E.
        0x0010:  0021 0000 0000 6411 3ebc 0a00 020f 0a00  .!....d.>.......
        0x0020:  0202 07d0 6403 000d 0000 7478 6f6e 65    ....d.....txone
```

要测试 `e1000_recv()` 是否能接收两个 packet（先是一个 ARP query，然后是一个 IP/UDP packet），可以：

在一个窗口中运行：

```bash
make qemu
```

在另一个窗口中运行：

```bash
python3 nettest.py rxone
```

`nettest.py rxone` 会通过 QEMU 向 xv6 发送一个 UDP packet。

实际上，QEMU 会先向 xv6 发送一个 ARP request；在 xv6 返回 ARP reply 之后，QEMU 才会把 UDP packet 转发给 xv6。

如果 `e1000_recv()` 工作正常，并把这些 packet 交给 `net_rx()`，`net.c` 应该打印：

```text
arp_rx: received an ARP packet
ip_rx: received an IP packet
```

`net.c` 中已经包含检测 QEMU 的 ARP request，并调用 `e1000_transmit()` 发送 reply 的代码。

因此，这个测试要求 `e1000_transmit()` 和 `e1000_recv()` 都能够工作。

此外，如果一切正常：

```bash
tcpdump -XXnr packets.pcap
```

应该产生类似这样的输出：

```text
reading from file packets.pcap, link-type EN10MB (Ethernet)
21:29:16.893600 ARP, Request who-has 10.0.2.15 tell 10.0.2.2, length 28
        0x0000:  ffff ffff ffff 5255 0a00 0202 0806 0001  ......RU........
        0x0010:  0800 0604 0001 5255 0a00 0202 0a00 0202  ......RU........
        0x0020:  0000 0000 0000 0a00 020f                 ..........
21:29:16.894543 ARP, Reply 10.0.2.15 is-at 52:54:00:12:34:56, length 28
        0x0000:  5255 0a00 0202 5254 0012 3456 0806 0001  RU....RT..4V....
        0x0010:  0800 0604 0002 5254 0012 3456 0a00 020f  ......RT..4V....
        0x0020:  5255 0a00 0202 0a00 0202                 RU........
21:29:16.902656 IP 10.0.2.2.61350 > 10.0.2.15.2000: UDP, length 3
        0x0000:  5254 0012 3456 5255 0a00 0202 0800 4500  RT..4VRU......E.
        0x0010:  001f 0000 0000 4011 62be 0a00 0202 0a00  ......@.b.......
        0x0020:  020f efa6 07d0 000b fdd6 7879 7a         ..........xyz
```

你的输出会略有不同，但里面应该包含这些字符串：

```text
ARP, Request
ARP, Reply
UDP
....xyz
```

如果上面两个测试都能正常工作，那么 `make grade` 应该会显示前两个测试已经通过。

## E1000 hints

对于 `e1000_transmit()`：

- 先在 `e1000_transmit()` 和 `e1000_recv()` 中加入一些打印语句，然后在 xv6 中运行 `nettest txone`。从打印结果中，你应该能看到 `nettest txone` 会触发对 `e1000_transmit()` 的调用。
- `e1000_dev.h` 中的 descriptor 定义使用的是 “legacy” transmit descriptor 格式（Section 3.3.3）。
- 首先，通过读取 `E1000_TDT` 控制寄存器，向 E1000 查询：它期望下一个 packet 被放在 TX ring 的哪个位置。
- 然后检查 ring 是否已经满了。如果 `E1000_TDT` 所指向 descriptor 中的 `E1000_TXD_STAT_DD` 没有被设置，说明 E1000 还没有完成该 descriptor 上一次对应的发送请求，因此应该返回错误。
- 否则，使用 `kfree()` 释放上一次从这个 descriptor 发送出去的 buffer（如果有的话）。
- 然后填写 descriptor。设置必要的 `cmd` flag（查看 E1000 手册的 Section 3.3）。
- 最后，把 ring 的位置更新为 `E1000_TDT + 1`，并对 `TX_RING_SIZE` 取模。

对于 `e1000_recv()`：

- 首先，通过读取 `E1000_RDT` 控制寄存器，再加 1 并对 `RX_RING_SIZE` 取模，找到下一个可能等待处理的已接收 packet 所在的 ring 下标。
- 然后通过检查 descriptor 的 `status` 字段中是否设置了 `E1000_RXD_STAT_DD`，判断是否有新的 packet。如果没有，就停止。
- 调用 `net_rx()`，把 packet buffer 交给网络栈。
- 然后用 `kalloc()` 分配一个新的 buffer，替换刚刚交给 `net_rx()` 的那个 buffer。把 descriptor 的 status bits 清零。
- 最后，把 `E1000_RDT` 寄存器更新为最后一个已经处理过的 ring descriptor 的下标。
- `e1000_init()` 会用 buffer 初始化 RX ring；你应该看看它是怎么做的，也许可以直接借用其中的一些代码。
- 最终，累计到达的 packet 数量一定会超过 ring 的大小（16）；确保你的代码能够正确处理这种情况。
- E1000 在一次 interrupt 中可能递送不止一个 packet；你的 `e1000_recv()` 应该能够处理这种情况。

你需要使用锁，因为可能出现以下情况：

- xv6 中不止一个进程同时使用 E1000；
- 内核线程正在使用 E1000 时，E1000 interrupt 到达。

## Part Two：UDP Receive（[中等难度](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html)）

UDP（User Datagram Protocol，用户数据报协议）允许不同 Internet host 上的用户进程交换单独的数据包（datagram）。

UDP 构建在 IP 之上。

用户进程通过指定一个 32-bit IP 地址，表示它想把 packet 发送到哪台 host。

每个 UDP packet 都包含：

- 一个 source port number；
- 一个 destination port number。

进程可以请求接收发往某些特定 port number 的 packet；发送时也可以指定 destination port number。

因此，如果两台不同 host 上的两个进程知道：

- 对方的 IP 地址；
- 对方正在监听的 port number；

那么它们就可以通过 UDP 通信。

例如，Google 在 IP 地址 `8.8.8.8` 的 host 上运行了一个 DNS name server，并监听 UDP port `53`。

在这个任务中，你要向 `kernel/net.c` 中添加代码，用来：

- 接收 UDP packet；
- 把它们排队；
- 让用户进程能够读取它们。

`net.c` 已经包含了用户进程发送 UDP packet 所需的代码，不过 `e1000_transmit()` 除外，因为这个函数需要由你实现。

你的任务是补全 `kernel/net.c` 中：

```c
ip_rx()
sys_recv()
sys_bind()
```

当 `make grade` 显示你的实现通过所有测试时，这一部分就完成了。

你可以自己运行和 `make grade` 相同的测试：

在一个窗口中运行：

```bash
python3 nettest.py grade
```

然后在另一个窗口中的 xv6 内运行：

```bash
nettest grade
```

如果一切正常，第一个窗口（xv6 外部）应该看到：

```text
$ python3 nettest.py grade
txone: OK
rxone: sending one UDP packet
```

而 xv6 窗口中应该看到：

```text
$ nettest grade
txone: sending one packet
arp_rx: received an ARP packet
ip_rx: received an IP packet
ping0: starting
ping0: OK
ping1: starting
ping1: OK
ping2: starting
ping2: OK
ping3: starting
ping3: OK
dns: starting
DNS arecord for pdos.csail.mit.edu. is 128.52.129.126
dns: OK
free: OK
```

这个 Lab 的 UDP system-call API 规范如下。

### `send`

```c
send(short sport, int dst, short dport, char *buf, int len)
```

这个系统调用向 IP 地址为 `dst` 的 host，以及该 host 上正在监听 `dport` 端口的进程发送一个 UDP packet。

这个 packet 的 source port number 是 `sport`。

这个 port number 会被报告给接收进程，以便接收进程可以回复发送者。

UDP packet 的内容（“payload”）是地址 `buf` 开始的 `len` 个字节。

返回值：

- 成功：`0`
- 失败：`-1`

### `recv`

```c
recv(short dport, int *src, short *sport, char *buf, int maxlen)
```

这个系统调用返回一个 destination port 为 `dport` 的 UDP packet 的 payload。

如果在调用 `recv()` 之前已经有一个或多个 packet 到达，那么它应该立即返回其中**最早等待的那个 packet**。

如果目前没有 packet 在等待，`recv()` 应该一直等待，直到有一个发往 `dport` 的 packet 到达。

对于某个给定 port，`recv()` 应该按照 packet 到达的顺序接收它们。

`recv()` 会：

- 把 packet 的 32-bit source IP address 复制到 `*src`；
- 把 packet 的 16-bit UDP source port number 复制到 `*sport`；
- 把 UDP payload 中最多 `maxlen` 个字节复制到 `buf`；
- 然后把这个 packet 从队列中移除。

系统调用返回：

- 实际复制的 UDP payload 字节数；
- 如果发生错误，则返回 `-1`。

### `bind`

```c
bind(short port)
```

进程在调用：

```c
recv(port, ...)
```

之前，应该先调用：

```c
bind(port)
```

如果一个 UDP packet 到达，而它的 destination port 从来没有传给过 `bind()`，那么 `net.c` 应该丢弃这个 packet。

之所以需要这个系统调用，是为了初始化 `net.c` 中用于存储到达 packet 的各种数据结构，使后续的 `recv()` 能够取得这些 packet。

### `unbind`

```c
unbind(short port)
```

你**不需要**实现这个系统调用，因为测试代码不会使用它。

不过如果你愿意，也可以实现它，以便和 `bind()` 对称。

传给这些系统调用的所有地址和 port number，以及由它们返回的地址和 port number，都必须使用 **host byte order**（见下文）。

除了 `send()` 之外，你需要提供这些系统调用的内核实现。

`user/nettest.c` 会使用这些 API。

为了让 `recv()` 能够工作，你需要向 `ip_rx()` 中加入代码。

对于每个收到的 IP packet，`net_rx()` 都会调用 `ip_rx()`。

`ip_rx()` 应该判断：

1. 到达的 packet 是否是 UDP；
2. 它的 destination port 是否已经传给 `bind()`。

如果两者都成立，就应该把这个 packet 保存到一个 `recv()` 能找到的地方。

不过，对于任意一个给定的 port，最多只能保存 **16 个 packet**。

如果已经有 16 个 packet 正在等待 `recv()`，那么新到达的、发往这个 port 的 packet 应该被丢弃。

这个规则的目的是防止一个速度很快或者恶意的发送者迫使 xv6 耗尽内存。

此外，如果某个 port 因为已经有 16 个 packet 在等待而开始丢包，这**不能影响其他 port 收到的 packet**。

`ip_rx()` 看到的 packet buffer 包含：

- 一个 14-byte Ethernet header；
- 接着是一个 20-byte IP header；
- 接着是一个 8-byte UDP header；
- 最后是 UDP payload。

这些结构对应的 C struct 定义都可以在 `kernel/net.h` 中找到。

Wikipedia 上也有 [IP header](https://en.wikipedia.org/wiki/Internet_Protocol_version_4#Header) 和 [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) 的说明。

真正用于生产环境的 IP/UDP 实现会非常复杂，需要处理 protocol option，并检查各种 invariant。

你只需要实现足够多的功能，使其能够通过 `make grade`。

你的代码需要查看：

IP header 中的：

```text
ip_p
ip_src
```

以及 UDP header 中的：

```text
dport
sport
ulen
```

你必须注意 **byte order（字节序）**。

Ethernet、IP 和 UDP header 中包含多字节整数的字段，在 packet 中会先放置最高有效字节（most significant byte）。

而 RISC-V CPU 在内存中存放多字节整数时，会先放置最低有效字节（least-significant byte）。

因此，当代码从 packet 中取出一个多字节整数时，必须重新排列这些字节。

这适用于：

- `short`（2 bytes）
- `int`（4 bytes）

你可以分别使用：

```c
ntohs()
ntohl()
```

处理 2-byte 和 4-byte 字段。

可以看看 `net_rx()`：它在查看 2-byte Ethernet type field 时已经给出了一个例子。

如果你的 E1000 代码中存在错误或遗漏，它们可能一直到 ping 测试时才开始暴露出来。

例如，ping 测试会收发足够多的 packet，使 descriptor ring 的下标发生 wrap around（绕回开头）。

一些提示：

- 创建一个 struct，用来记录已经 bind 的 port，以及这些 port 对应队列中的 packet。
- 参考 `kernel/proc.c` 中的：
  ```c
  sleep(void *chan, struct spinlock *lk)
  wakeup(void *chan)
  ```
  来实现 `recv()` 的等待逻辑。
- `sys_recv()` 要把 packet 复制到的目标地址都是虚拟地址；你需要把数据从 kernel 复制到当前 user process。
- 确保已经复制给用户的 packet，以及被丢弃的 packet，都要被正确释放。

## 提交 Lab

### Time spent

创建一个新文件：

```text
time.txt
```

其中只写一个整数，表示你在这个 Lab 上花费的小时数。

然后对这个文件执行：

```bash
git add time.txt
git commit
```

### Answers

如果这个 Lab 中包含需要回答的问题，把答案写进：

```text
answers-*.txt
```

然后对这些文件执行：

```bash
git add
git commit
```

### Submit

作业通过 Gradescope 提交。

你需要一个 MIT Gradescope 账号。

请在 Piazza 中查看加入课程所需的 entry code。

如果需要更多关于如何使用 course code 加入课程的帮助，可以参考[这个链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

准备好提交后，运行：

```bash
make zipball
```

它会生成：

```text
lab.zip
```

把这个 zip 文件上传到 Gradescope 中对应的作业即可。

如果你运行：

```bash
make zipball
```

时存在尚未 commit 的修改，或者存在未被 git 跟踪的文件，你会看到类似：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

检查上面的这些行，并确保 Lab solution 所需的所有文件都已经被 git 跟踪，也就是说，它们不应该出现在以 `??` 开头的行中。

你可以使用：

```bash
git add {filename}
```

让 git 开始跟踪你创建的新文件。

- 请运行 `make grade`，确保代码通过所有测试。Gradescope autograder 会使用同一套 grading program 给你的提交评分。
- 在运行 `make zipball` 之前，请 commit 所有修改过的源码。
- 你可以在 Gradescope 上查看提交状态，并下载已提交的代码。Gradescope 上显示的 Lab 分数就是你的最终 Lab 成绩。

## Optional Challenges（可选挑战）

- 在这个 Lab 中，网络栈使用 interrupt 来处理 ingress（进入的）packet，但不使用 interrupt 来处理 egress（出去的）packet。一个更复杂的策略是：先在软件中把 egress packet 排队，并且任意时刻只向 NIC 提供有限数量的 packet。然后可以依靠 TX interrupt 来重新填充 transmit ring。使用这种技术之后，就有可能为不同类型的 egress traffic 设置不同优先级。([easy](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))
- 提供的网络代码只部分支持 ARP。实现一个完整的 [ARP cache](https://tools.ietf.org/html/rfc826)。([moderate](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))
- E1000 支持多个 RX ring 和 TX ring。配置 E1000，为每个 core 提供一对 ring，并修改网络栈以支持多个 ring。这样有可能提升网络栈所能支持的吞吐量，同时减少锁竞争。([moderate](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))，但很难测试/测量。
- [ICMP](https://tools.ietf.org/html/rfc792) 可以提供网络 flow 失败的通知。检测这些通知，并把它们作为错误传递给用户进程。
- E1000 支持若干无状态硬件 offload，包括 checksum calculation、RSC 和 GRO。使用其中一种或多种 offload 来提高网络栈的吞吐量。([moderate](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))，但很难测试/测量。
- 这个 Lab 中的网络栈容易出现 receive livelock。根据课堂和阅读材料中的内容，设计并实现一种解决方案。([moderate](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))，但很难测试。
- 实现一个最小的 TCP stack，并用它下载一个网页。([hard](https://pdos.csail.mit.edu/6.828/2025/labs/guidance.html))

这些挑战中的一部分，目标是在 QEMU 下可能不明显、甚至难以测量的方面提高性能。
