# Distributed binary xml

I had a idea today, about two sites with the same binary xml.
They will have tcp connection established and will be able to send changes differential changes in both ways.

I will need "object structure" where objects will need GUID identification.

Each commited object snapshot will produce set of differences to connected stream.

## Useful features

It would be nice to be able to work in disconnected environment.

We will have to fill some transaction log - to be able to reply all changes on remote sites. But these transaction logs will need merge.
