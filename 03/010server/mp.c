#include "exp1.h"
#include "exp1lib.h"
#include <stdbool.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


bool echoBackLoop(int sock){
    char buf[1024];

  while(1){
    int ret = recv(sock, buf,1024,0);
    write(1,buf,ret);
    if(ret == 0){
      break;
    }
    send(sock,buf,ret,0);
  }
}

void printChildProcessStatus(pid_t pid, int status) {
  fprintf(stderr, "sig_chld_handler:wait:pid=%d,status=%d\n", pid, status);
  fprintf(stderr, "  WIFEXITED:%d,WEXITSTATUS:%d,WIFSIGNALED:%d,"
      "WTERMSIG:%d,WIFSTOPPED:%d,WSTOPSIG:%d\n", WIFEXITED(status),
      WEXITSTATUS(status), WIFSIGNALED(status), WTERMSIG(status),
      WIFSTOPPED(status),
      WSTOPSIG(status));
}

// シグナルハンドラによって子プロセスのリソースを回収する
void sigChldHandler(int sig) {
  // 子プロセスの終了を待つ
  int status = -1;
  pid_t pid = wait(&status);

  // 非同期シグナルハンドラ内でfprintfを使うことは好ましくないが，
  // ここではプロセスの状態表示に簡単のため使うことにする
  printChildProcessStatus(pid, status);
}



void acceptLoop(int sock) {

  // 子プロセス終了時のシグナルハンドラを指定
  struct sigaction sa;
  sigaction(SIGCHLD, NULL, &sa);
  sa.sa_handler = sigChldHandler;
  sa.sa_flags = SA_NODEFER;
  sigaction(SIGCHLD, &sa, NULL);

  while (true) {
    struct sockaddr_storage from;
    socklen_t len = sizeof(from);
    int acc = 0;
    if ((acc = accept(sock, (struct sockaddr *) &from, &len)) == -1) {
      // エラー処理
      if (errno != EINTR) {
        perror("accept");
      }
    } else {
      // クライアントからの接続が行われた場合
      char hbuf[NI_MAXHOST];
      char sbuf[NI_MAXSERV];
      getnameinfo((struct sockaddr *) &from, len, hbuf, sizeof(hbuf),
          sbuf, sizeof(sbuf),
          NI_NUMERICHOST | NI_NUMERICSERV);
      fprintf(stderr, "accept:%s:%s\n", hbuf, sbuf);

      // プロセス生成
      pid_t pid = fork();
      if (pid == -1) {
        // エラー処理
        perror("fork");
        close(acc);
      } else if (pid == 0) {
        // 子プロセス
        close(sock);  // サーバソケットクローズ
        echoBackLoop(acc);
        close(acc);    // アクセプトソケットクローズ
        acc = -1;
        _exit(1);
      } else {
        // 親プロセス
        close(acc);    // アクセプトソケットクローズ
        acc = -1;
      }

      // 子プロセスの終了処理
      int status = -1;
      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 終了した(かつSIGCHLDでキャッチできなかった)子プロセスが存在する場合
        // WNOHANGを指定してあるのでノンブロッキング処理
        // 各子プロセス終了時に確実に回収しなくても新規クライアント接続時に回収できれば十分なため．
        printChildProcessStatus(pid, status);
      }
    }
  }
}




int main(void){
    int sock_listen;
    int sock_client;
    struct sockaddr addr;
    int len = 0;
    
	sock_listen = exp1_tcp_listen(11111);
	sock_client = accept(sock_listen, &addr, (socklen_t*) &len);
    acceptLoop(sock_listen);
	close(sock_client);
	close(sock_listen);

	return 0;
}