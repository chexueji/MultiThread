#include "rwlock.h"
#include "errors.h"

#define THREADS 5
#define DATASIZE 15
#define ITERATIONS 10000

typedef struct thread_tag{
	int 	thread_num; //线程索引
	pthread_t	thread_id;//线程ID
	int	updates;//线程执行的读锁计数器
	int	reads;//线程执行的写锁计数器
	int 	interval;//在线程执行写操作之前，线程将重复读的循环次数
}thread_t;

typedef struct data_tag{
	rwlock_t	lock;
	int	data;
	int 	updates;//线程已经更新该元素的计数
}data_t;

thread_t	threads[THREADS];
data_t	data[DATASIZE];
//线程函数
void *thread_routine(void *arg)
{
	thread_t *self=(thread_t *)arg;
	int repeats=0;
	int iteration;
	int element=0;
	int status;
	
	for(iteration=0;iteration<ITERATIONS;iteration++){
		//当interval成员指定的间隔到来了 将更新data的值		
		if((iteration%self->interval)==0){
			status=rwl_writelock(&data[element].lock);//加写锁
			if(status)
				err_abort(status,"Write lock");
			data[element].data=self->thread_num;//线程索引值
			data[element].updates++;//修改更新次数加1
			self->updates++;//当前线程结构更细次数加1
			status=rwl_writeunlock(&data[element].lock);//解写锁
			if(status!=0)
				err_abort(status,"Write unlock");
		}else{
			status=rwl_readlock(&data[element].lock);//加共享读锁
			if(status!=0)
				err_abort(status,"Read lock");
			self->reads++;//修改读取次数
			if(data[element].data==self->thread_num)//如果data与num相等 记录重复次数
				repeats++;
			status=rwl_readunlock(&data[element].lock);
			if(status!=0)
				err_abort(status,"Read unlock");
		}
		element++;//data数组的索引
		if(element>=DATASIZE)
			element=0;
	}
	
	if(repeats>0)
		printf("Thread %d found unchanged elements %d times\n",self->thread_num,repeats);
	
	return NULL;
}
int main(int argc,char*argv[])
{
	int count;
	int data_count;
	int status;
	unsigned int seed=1;
	int thread_updates=0;
	int data_updates=0;
	//初始化data数组及读/写锁
	for(data_count=0;data_count<DATASIZE;data_count++){
		data[data_count].data=0;
		data[data_count].updates=0;
		status=rwl_init(&data[data_count].lock);
		if(status)
			err_abort(status,"Init rw lock");
	}
	//初始化线程数组 创建线程
	for(count=0;count<THREADS;count++){
		threads[count].thread_num=count;
		threads[count].updates=0;
		threads[count].reads=0;
		threads[count].interval=rand_r(&seed)%71;
		status=pthread_create(&threads[count].thread_id,NULL,thread_routine,(void*)&threads[count]);
		if(status)
			err_abort(status,"Create thread");
	}
	//依次连接线程 记录总的读锁次数和写锁次数
	for(count=0;count<THREADS;count++){
		status=pthread_join(threads[count].thread_id,NULL);
		if(status!=0)
			err_abort(status,"join thread");
		thread_updates+=threads[count].updates;
		printf("%02d:interval %d,updates %d,reads %d\n",count,threads[count].interval,threads[count].updates,threads[count].reads);
	}
	//输出data数组中总的更新次数 并且释放每个元素的读/写锁
	for(data_count=0;data_count<DATASIZE;data_count++){
		data_updates+=data[data_count].updates;
		printf("data %02d:value %d, %d udpates\n",data_count,data[data_count].data,data[data_count].updates);
		rwl_destroy(&data[data_count].lock);
	}
	printf("%d thread updates, %d data updates\n",thread_updates,data_updates);
	return 0;
}
