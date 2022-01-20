# Distributed binary xml

I had an idea today, about two sites with the same binary xml.
They will have tcp connection established and will be able to send differential changes in both ways.

I will need "object structure" where objects will need GUID identification.

Each commited object snapshot will produce set of differences to connected stream.

## Useful features

It would be nice to be able to work in disconnected environment.

We will have to fill some transaction log - to be able to replay all changes on remote sites. But these transaction logs will need merge. And it has risk of conflicts.


## Priorities

Each object can have exclusive owner - who will only change it's properties or there could be priorities - which could allow to automatically solve conflicts.

Each object could be locked for limited time - but timing is difficult in distributed environment.

Each object can be semilocked for limited time - it will get "1st", "2nd" ... lock and priority of change will be based on level of lock.

## Server - client

It would be nice to have ability to connect thin client to some kind of BIG database.

It would be something like:

    - CONNECT 192.168.45.45:150 #server
    - OPEN 2020/current.xb      #open database
    - 2020/current.xb> GET /HEADER /CONTROLLERS/CONTROLLER(Name like '%test%')/* AS C1
    
... local manipulation

    - 2020/current.xb> COMMIT 0x45640d0 0x5456460 
    - 2020/current.xb> CLOSE C1

    - 2020/current.xb> SUBSCRIBE /CONTROLLERS/*

## Minimal version

Peer to peer model. There will be local modifications, sending whole objects to remote site?

    LINK TO 192.168.45.45:150 2020/current.xb

    START TRANSACTION ON /CONTROLLERS/CONTROLLER(ID = 123)
      ... this will create local copy for future compare
      ... local manipulation
    COMMIT TRANSACTION
      ... compare current & snapshot
      ... produce diff
      ... send diff to all subscribers
    
    
Or it may be better to do changes only via methods - values can have flags 'changed' and changed values will be send.
But what about change A -> B -> A?

It will be hard to reference to element in a remote document.

I would need int64 guids - for exact referencing all tags.

But it can be enough to refrerence only repeating elements. Some elements can be only 1 in a object.

Attributes will not need any references.
   
32bit guid is enough for referencing inside 2G max files!


## New ideas

How can I modify xb or xbw file?

In xbw I can fully change simple values, add or remove nodes, attributes.

In xb I can change simple values and can issue a new copy of node, when node is changed. But there is limitation in reused nodes - these must be copied out and changed there. I would like to have some attribute of node, whether it has multiple references or not. When multiple references, node must be copied and then changed!

## Conflict resolving

There could exist a conflict of changes, when there is some delay between read - write.

For possibility of conflict resolving is full history of changes needed.

Could I just atomically add some changes to journal? What about different order of changes?

Is it possible to merge journals which were isolated for a long time?

So communication would be upstream and downstream of changes - all applied or rejected.

There would be only 1 main server. Failure or success could be judged only by main process.


