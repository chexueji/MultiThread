#include <pthread.h>
#include "errors.h"
#include "rwlock.h"
//锁初始化函数
int rwl_init(rwlock_t *rwl)
{
	int status;
	//初始化为0
	rwl->r_active=0;
	rwl->r_wait=rwl->w_wait=0;
	rwl->w_active=0;
	//初始化互斥量和条件变量
	status=pthread_mutex_init(&rwl->mutex,NULL);
	if(status)
		return status;
	status=pthread_cond_init(&rwl->read,NULL);	
	if(status!=0){
		pthread_mutex_destroy(&rwl->mutex);
		return status;
	}
	status=pthread_cond_init(&rwl->write,NULL);
	if(status!=0){
		pthread_cond_destroy(&rwl->read);
		pthread_mutex_destroy(&rwl->mutex);
		return status;
	}
	rwl->valid=RWLOCK_VALID;//标志锁为有效
	return 0;
}
//释放锁结构
int rwl_destroy(rwlock_t *rwl)
{
	int status,status1,status2;
	//读写锁无效 返回
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	
	status=pthread_mutex_lock(&rwl->mutex);//加锁互斥量
	if(status!=0)
		return status;
	//当前有正在读或写数据的锁 解锁互斥量返回
	if(rwl->r_active>0||rwl->w_active){
		pthread_mutex_unlock(&rwl->mutex);
		return EBUSY;
	}
	//当前有等待的读/写线程 解锁互斥量返回
	if(rwl->r_wait>0||rwl->w_wait>0){
		pthread_mutex_unlock(&rwl->mutex);
		return EBUSY;	
	}
	
	rwl->valid=0;//将读/写锁标志为无效
	status=pthread_mutex_unlock(&rwl->mutex);//解锁互斥量
	if(status!=0)
		return status;
	//释放互斥量和条件变量
	status=pthread_mutex_destroy(&rwl->mutex);
	status1=pthread_cond_destroy(&rwl->read);
	status2=pthread_cond_destroy(&rwl->write);
	return (status!=0?status
		:(status1!=0?status1:status2));
}
//读锁的清除处理函数 当一个等待被取消时，等待线程需要相应地减少等待读或写锁的线程数并解锁互斥量
static void rwl_readcleanup(void *arg)
{
	rwlock_t * rwl=(rwlock_t *)arg;
	rwl->r_wait--;
	pthread_mutex_unlock(&rwl->mutex);
}
//写锁清除处理函数 当条件标量的等待操作被取消时执行清除处理函数
static void rwl_writecleanup(void *arg)
{
	rwlock_t *rwl=(rwlock_t *)arg;
	rwl->w_wait--;//此处是否与
	pthread_mutex_unlock(&rwl->mutex);
}
//为读操作加锁读/写锁
int rwl_readlock(rwlock_t *rwl)
{
	int status;
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	status=pthread_mutex_lock(&rwl->mutex);
	if(status)
		return status;
	//如果当前写线程是活动的 则等待它广播条件变量read
	if(rwl->w_active){
		rwl->r_wait++;//等待读线程数目加1
		pthread_cleanup_push(rwl_readcleanup,(void*)rwl);//加载清除处理函数
		while(rwl->w_active){
			status=pthread_cond_wait(&rwl->read,&rwl->mutex);//等待条件变量read
			if(status)
				break;
		}
		pthread_cleanup_pop(0);//不执行清除处理函数
	}
	if(status==0)
		rwl->r_active++;//当前读线程数目加1 加锁成功
	pthread_mutex_unlock(&rwl->mutex);
	return status;
}
//为读操作的试加锁读/写锁 加锁不成功则立即返回EBUSY
int rwl_readtrylock(rwlock_t *rwl)
{
	int status,status2;
	
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	
	status=pthread_mutex_lock(&rwl->mutex);
	if(status)
		return status;
	//有写线程活动
	if(rwl->w_active)
		status=EBUSY;
	else
		rwl->r_active++;//读线程数目加1 加锁成功
	
	status2=pthread_mutex_unlock(&rwl->mutex);
	return (status!=0?status:status);
}
//解锁读/写锁的读操作
int rwl_readunlock(rwlock_t *rwl)
{
	int status,status2;
	
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	status=pthread_mutex_lock(&rwl->mutex);//加锁互斥量
	if(status)
		return status;
	
	rwl->r_active--;//读操作的活动线程数目减1
	//如果当前没有读操作线程 并且有等待的写线程 则发write条件变量信号
	if(rwl->r_active==0&&rwl->w_wait>0)
		status=pthread_cond_signal(&rwl->write);
	status2=pthread_mutex_unlock(&rwl->mutex);//解锁互斥量
	return (status2==0?status:status2);
}
//为写操作加锁读/写锁
int rwl_writelock(rwlock_t *rwl)
{
	int status;
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	status=pthread_mutex_lock(&rwl->mutex);
	if(status)
		return status;
	//有写线程或者读线程
	if(rwl->w_active||rwl->r_active>0){
		rwl->w_wait++;//等待写线程数目加1
		pthread_cleanup_push(rwl_writecleanup,(void*)rwl);//加载读清除处理函数
		while(rwl->w_active||rwl->r_active>0){
			status=pthread_cond_wait(&rwl->write,&rwl->mutex);//等待write条件变量信号
			if(status!=0)
				break;
		}
		pthread_cleanup_pop(0);//不执行清除处理函数
		rwl->w_wait--;//等待写线程减159行的函数执行重复 TODO
	}
	if(status==0)
		rwl->w_active=1;//写进程数目加1
	pthread_mutex_unlock(&rwl->mutex);
	return status;
}
//为写操作试加锁读/写锁 加锁不成功则立即返回EBUSY
int rwl_writetrylock(rwlock_t * rwl)
{
	int status,status2;
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	
	status=pthread_mutex_lock(&rwl->mutex);
	if(status!=0)
		return status;
	if(rwl->w_active||rwl->r_active>0)
		status=EBUSY;
	else
		rwl->w_active=1;
	status2=pthread_mutex_unlock(&rwl->mutex);
	return(status!=0?status:status2);
}
//解锁读/写锁的写锁 读进程优先于写进程
int rwl_writeunlock(rwlock_t *rwl)
{
	int status;
	
	if(rwl->valid!=RWLOCK_VALID)
		return EINVAL;
	status=pthread_mutex_lock(&rwl->mutex);
	if(status)
		return status;
	rwl->w_active=0;//置0
	//如果当前有正在等待的读进程 广播条件变量read 否则通知正在等待的写线程
	if(rwl->r_wait>0){
		status=pthread_cond_broadcast(&rwl->read);//广播read
		if(status!=0){
			pthread_mutex_unlock(&rwl->mutex);
			return status;
		}
	}else if(rwl->w_wait>0){
		status=pthread_cond_signal(&rwl->write);//通知write
		if(status!=0){
			pthread_mutex_unlock(&rwl->mutex);
			return status;
		}
	}
	status=pthread_mutex_unlock(&rwl->mutex);
	return status;
}
