FROM new:test
COPY main.c /home
EXPOSE 9090 8080
CMD ls
