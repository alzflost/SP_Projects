/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct item {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
} item;

typedef struct tree_node* tree_pointer;

typedef struct tree_node {
    item data;
    tree_pointer left_child, right_child;
}tree_node;

tree_pointer root;
char print_data[20000];

sem_t mutex, w;
int read_cnt = 0;
int byte_cnt = 0;
int th_cnt = 0;

void command(char* buf, int connfd);
void stock_info(void);
tree_pointer tree_insert(tree_pointer cur, item it);
void tree_print(int fd, tree_pointer cur);
tree_pointer search_tree(tree_pointer cur, int id);
int stock_buy(int stock_id, int q);
int stock_sell(int stock_id, int q);
void update_stock(void);
void print_fp_tree(FILE* fp, tree_pointer cur);
void init_sema(void);
void *thread(void *vargp);

int main(int argc, char **argv) 
{
    int listenfd, *connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    stock_info();
    listenfd = Open_listenfd(argv[1]);
    init_sema();
    while (1) {
	clientlen = sizeof(struct sockaddr_storage); 
	connfd = Malloc(sizeof(int));
	*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
	th_cnt++;
	Pthread_create(&tid, NULL, thread, connfd);
	// echo(connfd);
	// Close(connfd);
    }
    exit(0);
}

void *thread(void* vargp){
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);

    rio_t rio;
    int n;
    char buf[MAXLINE];

    Rio_readinitb(&rio, connfd);

    while ( (n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
	P(&w);
	byte_cnt += n;
	V(&w);
	printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
	command(buf, connfd);
    }
    Close(connfd);
    P(&w);
    th_cnt--;
    V(&w);
    printf("Connection with fd %d Closed, %d\n", connfd, th_cnt);
    if (th_cnt == 0){
	update_stock();
    }
}

void init_sema(void){
    sem_init(&mutex, 0, 1);
    sem_init(&w, 0, 1);
}

void command(char* buf, int connfd){
    if (strcmp(buf, "show\n") == 0){
	P(&mutex);
	read_cnt++;
	if (read_cnt == 1){
	    P(&w);
	}
	V(&mutex);

	tree_print(connfd, root);

	P(&mutex);
	read_cnt--;
	if (read_cnt == 0){
	    V(&w);
	}
	V(&mutex);

	Rio_writen(connfd, print_data, MAXLINE);
    }
    else if (strcmp(buf, "exit\n") == 0){
	update_stock();
    }
    else {
	char com[5];
	int stock_id = 0;
	int quantity = 0;
	int success = 1;
	sscanf(buf, "%s %d %d", &com, &stock_id, &quantity);
	if (strcmp(com, "buy") == 0){
	    P(&w);
	    success = stock_buy(stock_id, quantity);
	    if (success == 1){
		strcpy(print_data, "[buy] success\n");
	    }
	    else {
		strcpy(print_data, "Not enough left stock\n");
	    }
	    V(&w);
	}
	else if (strcmp(com, "sell") == 0){
	    P(&w);
	    stock_sell(stock_id, quantity);
	    strcpy(print_data, "[sell] success\n");
	    V(&w);
	}
	else {
	    strcpy(print_data, "error\n");
	}
	Rio_writen(connfd, print_data, MAXLINE);
	memset(print_data, 0, sizeof(print_data));
    }	
}

void stock_info(){
    root = (tree_pointer)malloc(sizeof(tree_node));
    FILE* fp;
    fp = fopen("stock.txt", "r+");
    int s_id, s_left, s_price;
    int isStart = 1;
    while (fscanf(fp, "%d %d %d", &s_id, &s_left, &s_price) != EOF) {
	item new_item;
	new_item.ID = s_id;
	new_item.left_stock = s_left;
	new_item.price = s_price;
	new_item.readcnt = 0;
	sem_init(&(new_item.mutex), 0, 1);
	if (isStart) {
	    root->data = new_item;
	    isStart = 0;
	}
	root = tree_insert(root, new_item);
    }
    close(fp);
}

tree_pointer search_tree(tree_pointer cur, int id){
    if (cur == NULL){
	// printf("?\n");
	return cur;
    }
    if ((cur->data).ID < id){
	cur = search_tree(cur->right_child, id);
    }
    else if ((cur->data).ID > id){
	cur = search_tree(cur->left_child, id);
    }
    else {
	return cur;
    }
    return cur;
}

int stock_buy(int stock_id, int q){
    tree_pointer change_node;
    change_node = search_tree(root, stock_id);
    // P(&((change_node->data).mutex));
    if (q > (change_node->data).left_stock){
	return 0;
    }
    (change_node->data).left_stock -= q;
    // V(&((change_node->data).mutex));
    return 1;
}

int stock_sell(int stock_id, int q){
    tree_pointer change_node;
    change_node = search_tree(root, stock_id);
    P(&((change_node->data).mutex));
    (change_node->data).left_stock += q;
    V(&((change_node->data).mutex));
    return 1;
}

tree_pointer tree_insert(tree_pointer cur, item it){
    if (cur == NULL){
	cur = (tree_pointer)malloc(sizeof(tree_node));
	cur->data = it;
	cur->left_child = NULL;
	cur->right_child = NULL;
    }
    else {
	if (it.ID < (cur->data).ID){
	    cur->left_child = tree_insert(cur->left_child, it);
	}
	else if (it.ID > (cur->data).ID) {
	    cur->right_child = tree_insert(cur->right_child, it);
	}
    }
    return cur;
}

void tree_print(int fd, tree_pointer cur){
    if (cur == NULL){
	return;
    }
    char temp1[10]; char temp2[10]; char temp3[10];
    sprintf(temp1, "%d ", (cur->data).ID);
    sprintf(temp2, "%d ", (cur->data).left_stock);
    sprintf(temp3, "%d\n", (cur->data).price);
    strcat(print_data, temp1); strcat(print_data, temp2); strcat(print_data, temp3);
    tree_print(fd, cur->left_child);
    tree_print(fd, cur->right_child);
    return;
}

void update_stock(void){
    FILE* fp;
    fp = fopen("stock.txt", "w");
    print_fp_tree(fp, root);
    fclose(fp);
}

void print_fp_tree(FILE* fp, tree_pointer cur){
    if (cur == NULL) return;
    fprintf(fp, "%d %d %d\n", (cur->data).ID, (cur->data).left_stock, (cur->data).price);
    print_fp_tree(fp, cur->left_child);
    print_fp_tree(fp, cur->right_child);
}

/* $end echoserverimain */
