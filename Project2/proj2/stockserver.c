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

int stock_pool_cnt = 0;

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

int byte_cnt = 0;
int client_cnt = 0;

void echo(int connfd);
void stock_info(void);
tree_pointer tree_insert(tree_pointer cur, item it);
void tree_print(int fd, tree_pointer cur);
tree_pointer search_tree(tree_pointer cur, int id);
int stock_buy(int stock_id, int q);
int stock_sell(int stock_id, int q);
void update_stock(void);
void print_fp_tree(FILE* fp, tree_pointer t);

void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for (i=0; i<FD_SETSIZE; i++){
	p->clientfd[i] = -1;
    }
    client_cnt = 0;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for (i=0; i<FD_SETSIZE; i++){
	if(p->clientfd[i] < 0) {
	    p->clientfd[i] = connfd;
	    client_cnt++;
	    Rio_readinitb(&p->clientrio[i], connfd);
	    
	    FD_SET(connfd, &p->read_set);

	    if (connfd > p->maxfd){
		p->maxfd = connfd;
	    }
	    if (i > p->maxi){
		p->maxi = i;
	    }
	    break;
	}
    }
    if (i == FD_SETSIZE){
	app_error("add_client error : Too many clients.");
    }
}

void check_clients(pool *p){
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++){
	connfd = p->clientfd[i];
	rio = p->clientrio[i];

	if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
	    p->nready--;
	    if (( n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		byte_cnt += n;
		printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
		
		//Rio_writen(connfd, buf, n);
		if (strcmp(buf, "show\n") == 0){
		    tree_print(connfd, root);
		    // printf("show complete\n");
		}
		else if (strcmp(buf, "exit\n") == 0){
		    Rio_writen(connfd, "exit\n", MAXLINE);
		    client_cnt--;
		    return 0;
		}
		else {
		    char command[10];
		    int stock_id = 0;
		    int quantity = 0;
		    int success = 1;
		    sscanf(buf, "%s %d %d", &command, &stock_id, &quantity);
		    if (strcmp(command, "buy") == 0){
			success = stock_buy(stock_id, quantity);
			if (success == 1){
			    strcpy(print_data, "[buy] success\n");
			    // update_stock();
			    // printf("buy success\n");
			}
			else {
			    // printf("buy fail\n");
			    strcpy(print_data, "Not enough left stock\n");
			}
		    }
		    else {
			success = stock_sell(stock_id, quantity);
			// printf("sell success\n");
			// update_stock();
			strcat(print_data, "[sell] success\n");
		    }
		}
		Rio_writen(connfd, print_data, MAXLINE);
		memset(print_data, 0, sizeof(print_data));
	    }
	    else {
		Close(connfd);
		FD_CLR(connfd, &p->read_set);
		p->clientfd[i] = -1;
		client_cnt--;
	    }
	}
    }
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    stock_info();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    while (1) {
	pool.ready_set = pool.read_set;
	pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

	if (FD_ISSET(listenfd, &pool.ready_set)) {
    	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	    add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
	}
	check_clients(&pool);
	
	if (client_cnt == 0){
	    update_stock();
	}
	
	//echo(connfd);
	// Close(connfd);
    }
    exit(0);
}

void stock_info(){
    root = (tree_pointer)malloc(sizeof(tree_node));
    FILE* fp;
    fp = fopen("stock.txt", "r");
    int s_id, s_left, s_price;
    int isStart = 1;
    while (fscanf(fp, "%d %d %d", &s_id, &s_left, &s_price) != EOF) {
	item new_item;
	new_item.ID = s_id;
	// printf("%d\n", new_item.ID);
	new_item.left_stock = s_left;
	new_item.price = s_price;
	new_item.readcnt = 0;
	if (isStart){
	    root->data = new_item;
	    isStart = 0;
	}
	root = tree_insert(root, new_item);
	// printf("%d done\n", new_item.ID);
    }
    // tree_print(root);
    close(fp);
    return;
}

tree_pointer search_tree(tree_pointer cur, int id){
    if (cur == NULL){
	printf("?\n");
	return cur;
    }
    if ((cur->data).ID < id){
	cur = search_tree(cur->right_child, id);
    }
    else if ((cur->data.ID > id)){
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
    if (q > (change_node->data).left_stock ){
	return 0;
    }
    (change_node->data).left_stock -= q;
    return 1;
}

int stock_sell(int stock_id, int q){
    tree_pointer change_node;
    change_node = search_tree(root, stock_id);
    (change_node->data).left_stock += q;
    return 1;
}

tree_pointer tree_insert(tree_pointer cur, item it){
    // printf("%d", (cur->data).ID);
    if (cur == NULL){
	cur = (tree_pointer)malloc(sizeof(tree_node));
	cur->data = it;
	cur->left_child = NULL;
	cur->right_child = NULL;
	return cur;
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
	// printf("end");
	return;
    }
    char temp1[30]; char temp2[10]; char temp3[10];
    sprintf(temp1, "%d ", (cur->data).ID);
    sprintf(temp2, "%d ", (cur->data).left_stock);
    sprintf(temp3, "%d\n", (cur->data).price);
    strcat(print_data, temp1); strcat(print_data, temp2); strcat(print_data, temp3);
    // printf("%d %d %d\n", cur->data.ID, cur->data.left_stock, cur->data.price);
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
