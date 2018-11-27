#include <pthread.h>

typedef struct rwlock_tag{
	pthread_mutex_t mutex;
	pthread_cond_t read;
	pthread_cond_t write;
	int 	valid;//标志lock结构是否有效
	int 	r_active;//正在读取数据的线程数
	int 	w_active;//现在写数据的线程数目
	int	r_wait;//等待读的线程数目
	int 	w_wait;//等待写的线程数目
}rwlock_t;

#define RWLOCK_VALID 0xfacade
//静态初始化宏
#define RWL_INITIALIAER \
	{PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER, \	
	PTHREAD_COND_INITIALIZER,RWLOCK_VALID,0,0,0,0}

extern int rwl_init(rwlock_t *rwlock);//初始化
extern int rwl_destroy(rwlock_t *rwlock);//释放
extern int rwl_readlock(rwlock_t *rwlock);//读加锁
extern int rwl_readtrylock(rwlock_t *rwlock);//读试加锁
extern int rwl_readunlock(rwlock_t *rwlock);//共享读解锁
extern int rwl_writelock(rwlock_t *rwlock);//写加锁
extern int rwl_writetrylock(rwlock_t *rwlock);//写试加锁
extern int rwl_writeunlock(rwlock_t *rwlock);//写解锁
