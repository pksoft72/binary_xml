# Distributed binary xml

I had a idea today, about two sites with the same binary xml.
They will have tcp connection established and will be able to send changes differential changes in both ways.

I will need "object structure" where objects will need GUID identification.

Each commited object snapshot will produce set of differences to connected stream.

## Useful features

It would be nice to be able to work in disconnected environment.

We will have to fill some transaction log - to be able to reply all changes on remote sites. But these transaction logs will need merge.


## Priorities

Each object can have exclusive owner - who will only change it's properties or there could be priorities - which could allow to automatically solve conflicts.

## Server - client

It would be nice to have ability to connect thin client to some kind of BIG database.

It would be something like:

    - CONNECT 192.168.45.45:150 #server
    - OPEN 2020/current.xb      #open database
    - GET /HEADER /CONTROLLERS/CONTROLLER(Name like '%test%')/*
    
... local manipulation

    - COMMIT 0x45640d0 0x5456460 

    - SUBSCRIBE ...
   

