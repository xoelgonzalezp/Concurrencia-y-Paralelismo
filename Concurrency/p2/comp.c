#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 1
#define COMPRESS 1
#define DECOMPRESS 0


// Struct used for the reader thread (This thread is used to read from the input file and insert chunks into the input queue)
typedef struct {
    queue in; 
    int chunk_size; // Size of each chunk
    int fd; // File descriptor
} reader_arguments;


// Struct used for the writer thread (This thread is used to remove from the output queue compressed chunks and adding them to the output file)
typedef struct {
    queue out;
    archive ar;
    int chunks;
} writer_arguments;

// Struct used for the workers (These threads are used to remove chunks from the input queue, compress them and insert them into the output queue)
typedef struct {
    queue in;
    queue out;
    int num_threads; 
} worker_arguments;


// Reader thread
void *reader(void *args_) {
    reader_arguments *reader_args = (reader_arguments *)args_;
    int num_chunks = 0;

    while (1) {

            chunk ch = alloc_chunk(reader_args->chunk_size); // We allocate memory for a new chunk and its data taking into account its size
            int offset = lseek(reader_args->fd, 0, SEEK_CUR); // We obtain the current offset in the file
            ch->size = read(reader_args->fd, ch->data, reader_args->chunk_size); // returns 0 when it gets to the end

            if (ch->size == 0) { //If we reached the end of the archive, we have finished the insertions in the input queue
                free_chunk(ch);
                break; 
            }
            
            ch->num = num_chunks++;
            ch->offset = offset; 

            q_insert(reader_args->in, ch, -1); // Reader inserts into input queue the chunks
    }
    q_set_all_finished(reader_args->in); //When the reader finishes inserting all the chunks, we set the input queue as finished
    return NULL;
}

// Writer thread
void *writer(void *arg) {
    writer_arguments *writer_args = (writer_arguments *)arg;
    int n_chunks = 0;

    while (1) {

        if (n_chunks == writer_args->chunks) { //If the writer has removed the total number of chunks from the output queue and added them to the archive, we stop
                break;
            }

        chunk ch = q_remove(writer_args->out, -1); //Removes from the output queue the compressed chunks inserted by the workers
        n_chunks++;
        if (ch != NULL) { 
           
            add_chunk(writer_args->ar, ch);  //If the chunk is not null, it adds into the archive that chunk
            free_chunk(ch); //After being added to the archive, its memory is freed
        }
    }
    return NULL;
}


// Worker thread
void *worker(void *arg) {
    worker_arguments *worker_args = (worker_arguments *)arg;
    chunk ch, res;

    srand(time(NULL));

    while (1) {
        ch = q_remove(worker_args->in, -1); //The worker removes a chunk from the input queue

        if (ch == NULL) {
            if (q_finished(worker_args->in) && q_elements(worker_args->in) == 0) { //We verify if the reader set the queue as finished and if the queue is empty, if that is the case, we stop
               
                break;
            }
            continue;
        }

        res = zcompress(ch); //Before being inserted into the output queue, it is compressed and stored in res
        free_chunk(ch); //After that, allocated memory for the original chunk is freed

        q_insert(worker_args->out, res, -1); //Inserts the compressed chunk into the output queue
    }

    return NULL;
}


// Start reader thread
void start_reader_thread(pthread_t *thread, reader_arguments *reader_arguments, int fd, queue in, int chunk_size) {
    reader_arguments->in = in;
    reader_arguments->chunk_size = chunk_size;
    reader_arguments->fd = fd;

    if (pthread_create(thread, NULL, reader, reader_arguments) != 0) {
        perror("Error creating reader thread");
        exit(EXIT_FAILURE);
    }
}

// Start writer thread
void start_writer_thread(pthread_t *thread, writer_arguments *writer_arguments, queue out, archive ar, int chunks) {
    writer_arguments->out = out;
    writer_arguments->ar = ar;
    writer_arguments->chunks = chunks;

    if (pthread_create(thread, NULL, writer, writer_arguments) != 0) {
        perror("Error creating writer thread");
        exit(EXIT_FAILURE);
    }
}

// Start workers
pthread_t *start_worker_threads(queue in, queue out, int num_threads) {
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    worker_arguments *worker_args = malloc(num_threads * sizeof(worker_arguments));

    for (int i = 0; i < num_threads; i++) {
        worker_args[i].in = in;
        worker_args[i].out = out;
        worker_args[i].num_threads = num_threads;
        pthread_create(&threads[i], NULL, worker, &worker_args[i]);
    }

    return threads;
}

// Used to wait for all workers to finish
void wait(pthread_t *threads, int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

// Function to compress a file
void comp(struct options opt) {
    int fd, chunks;
    struct stat st;
    char comp_file[256];
    archive ar;
    queue in, out;
    pthread_t *worker_threads;
    pthread_t reader_thread,writer_thread;
    reader_arguments reader_arguments;
    writer_arguments writer_arguments;

    if ((fd = open(opt.file, O_RDONLY)) == -1) {
        perror("Cannot open input file");
        exit(EXIT_FAILURE);
    }

    fstat(fd, &st);
    chunks = st.st_size / opt.size + (st.st_size % opt.size ? 1 : 0);

    printf("There are %d chunks.\n", chunks);
    snprintf(comp_file, sizeof(comp_file), "%s%s", opt.out_file ? opt.out_file : opt.file, ".ch");
    ar = create_archive_file(comp_file);
    in = q_create(opt.queue_size);
    out = q_create(opt.queue_size);
    printf("Queue size is %d.\n",opt.queue_size);

    start_reader_thread(&reader_thread, &reader_arguments, fd, in, opt.size); //We start the reader thread
    start_writer_thread(&writer_thread, &writer_arguments, out, ar, chunks); //We start the writer thread
    worker_threads = start_worker_threads(in, out, opt.num_threads); //We start the workers

    wait(worker_threads, opt.num_threads); //Wait for all workers to finish
    pthread_join(reader_thread, NULL); //Wait for reader thread to finish
    pthread_join(writer_thread, NULL); // Wait for writer thread to finish

    printf("All threads have finished: File compressed.\n");
    free(worker_threads);
    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);

}

//Function to decompress a file
void decomp(struct options opt) {
    int fd, i;
    char uncomp_file[256];
    archive ar;
    chunk ch, res;

    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    for(i=0; i<chunks(ar); i++) {
        ch = get_chunk(ar, i);

        res = zdecompress(ch);
        free_chunk(ch);

        lseek(fd, res->offset, SEEK_SET);
        write(fd, res->data, res->size);
        free_chunk(res);
    }

    close_archive_file(ar);
    close(fd);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}