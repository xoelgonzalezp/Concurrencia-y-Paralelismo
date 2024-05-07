
-module(comp).
-export([comp/1,comp/2,decomp/1,decomp/2,comp_proc/2, comp_proc/3,decomp_proc/2,decomp_proc/3,comp_loop/2,decomp_loop/2]).
-define(DEFAULT_CHUNK_SIZE, 1024*1024).

%%Single-threaded compression and decompression

comp(File) -> 
    comp(File, ?DEFAULT_CHUNK_SIZE).

decomp(Archive) ->
    decomp(Archive, string:replace(Archive, ".ch", "", trailing)).

%%Concurrent compression and decompresion 

comp_proc(File, Procs) ->
    comp_proc(File, ?DEFAULT_CHUNK_SIZE, Procs).

decomp_proc(Archive,Procs) -> 
    decomp_proc(Archive, string:replace(Archive, ".ch", "", trailing), Procs).


%% Single-threaded functions comp/2 and decomp/2

comp(File, Chunk_Size) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    try
                        comp_loop(Reader, Writer),
                        Reader ! stop,  
                        Writer ! stop  
                    catch  
                        throw:{error, Reason} ->
                            io:format("Error reading input file: ~w~n", [Reason]),
                            Reader ! stop,  
                            Writer ! abort        
                    end;
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason]),
                    {error, {output_file_error, Reason}}
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason]),
            {error, {input_file_error, Reason}}
    end.


decomp(Archive, Output_File) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    try
                        decomp_loop(Reader, Writer),
                        Reader ! stop,  
                        Writer ! stop
                    catch
                        throw:{error, Reason} ->
                            io:format("Error reading input file: ~w~n", [Reason]),
                            Reader ! stop,  
                            Writer ! abort 
                    end;
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason]),
                    {error, {output_file_error, Reason}}
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason]),
            {error, {input_file_error, Reason}}
    end.



%% Concurrent functions comp_proc/3 and decomp_proc/3

comp_proc(File, Chunk_Size, Procs) ->
    io:format("Starting compression process for file ~p with chunk size ~p using ~p workers.~n", [File, Chunk_Size, Procs]),
    case {file_service:start_file_reader(File, Chunk_Size), archive:start_archive_writer(File ++ ".ch")} of
        {{ok, Reader}, {ok, Writer}} ->
            worker(Reader, Writer,Procs,true),
            Reader ! stop,
            Writer ! stop;
        {{error, Reason}, _} ->
            io:format("Could not open input file: ~w~n", [Reason]);
        {_, {error, Reason}} ->
            io:format("Could not open output file: ~w~n", [Reason])
    end.


decomp_proc(Archive, Output_File, Procs) ->
    io:format("Starting decompression process for archive ~p to ~p using ~p workers.~n", [Archive, Output_File, Procs]),
    case {archive:start_archive_reader(Archive), file_service:start_file_writer(Output_File)} of
        {{ok, Reader}, {ok, Writer}} ->
            worker(Reader, Writer, Procs,false),
            Reader ! stop,
            Writer ! stop;
        {{error, Reason}, _} ->
            io:format("Could not open input file: ~w~n", [Reason]);
        {_, {error, Reason}} ->
            io:format("Could not open output file: ~w~n", [Reason])
    end.


%% comp_loop/2 and decomp_loop/2

comp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()}, %%worker process asks the reader for a chunk
    receive
        {chunk, Num, Offset, Data} -> %%if the worker receives a chunk
            io:format("Process ~p is compressing chunk number ~p ~n", [self(), Num]),
            Comp_Data = compress:compress(Data), %%compresses data
            Writer ! {add_chunk, Num, Offset, Comp_Data}, %%worker sends message to writer with the compressed chunk
            comp_loop(Reader, Writer); %%then, worker calls again comp_loop to process another chunk.
        eof ->
            io:format("Process ~p has completed compression.~n", [self()]);
        {error, Reason} ->
            io:format("Process ~p encountered an error: ~w.~n", [self(), Reason]),
            throw({error, Reason}) 
    end.


decomp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()}, 
    receive
        {chunk, _Num, Offset, Data} ->
            io:format("Process ~p is decompressing chunk at offset ~p~n", [self(), Offset]),
            Decompressed_Data = compress:decompress(Data),
            Writer ! {write_chunk, Offset, Decompressed_Data},
            decomp_loop(Reader, Writer);
        eof ->
            io:format("Process ~p has completed decompression.~n", [self()]);
        {error, Reason} ->
            io:format("Process ~p encountered an error: ~w.~n", [self(), Reason]),
            throw({error, Reason}) 
    end.


%% Reader process, writer process, number of Procs(workers), boolean to know if we have to compress or decompress
%% Function which starts workers
worker(Reader, Writer, Procs, IsCompression) ->
    StartWorker = fun() -> %%anonymous function to start a worker with spawn_link, which creates a process and links it to its Parent process(comp_proc or decomp_proc)
                           %%If a worker fails during its execution, erlang will notify the Parent process with an exit signal due to this link.
                           %%If the Parent process fail during its execution, workers will finish its execution also.
        spawn_link(fun() -> 
            receive  
                {start, Parent} ->   %%when workers receive the start message from the process the can start its task
                    try 
                        if IsCompression ->  
                            comp_loop(Reader, Writer); 
                        true ->  %%if the boolean is set to false workers will do decompression
                            decomp_loop(Reader, Writer)
                        end,
                        notify_parent(Parent, {done, self()})
                    catch
                        throw:{error, Reason} -> 
                            notify_parent(Parent, {error, self(), Reason})
                    end
            end
        end)
    end,
    WorkersList = [StartWorker() || _ <- lists:seq(1, Procs)], %%lists:seq does a loop of integer numbers from 1 to N, on each iteration startworker() is called and the result is added to workerslist ,
                                                               % _<- indicates we are not using the value of that position, we are iterating over the list.
                                                               %%a worker is created with the StartWorker() function
    lists:foreach(fun(Worker) -> Worker ! {start, self()} end, WorkersList), %%in each element of the list(worker pid), we send a start message with the pid of the process(comp_proc or decomp_proc) which called the worker function
                                                                             %% for each worker that was previously created with spawn_link, notifying them that they can start doing its work
                                                                             
    wait(WorkersList).
   


%%wait for all workers to finish
wait(Workers) ->
    case Workers of
        [] -> %%if the list is empty of if we finished iterating over the list recursively, all workers have finished
            ok;
        [_Head | Tail] -> %%if we receive a done or error message from head, we continue iterating over the tail, if the message does not match one of these,
                          %%we wait in the same worker for a valid message
            receive
                {done, WorkerPid} -> 
                    io:format("Received completion message from worker with PID ~p.~n", [WorkerPid]),
                    wait(Tail);
                {error, WorkerPid, Reason} ->
                    io:format("Worker ~p encountered an error: ~p.~n", [WorkerPid, Reason]),
                    wait(Tail);
                _ -> 
                 
                    wait(Workers)
            end
    end.


notify_parent(Parent, Message) -> %%function to notify parent process
    Parent ! Message.

