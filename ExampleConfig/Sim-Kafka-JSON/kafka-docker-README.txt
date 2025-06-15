
run 'sudo docker compose up -d' to use the docker-compose.yml

Test Consumer:

	docker exec --interactive --tty broker kafka-console-consumer --bootstrap-server localhost:9092 --topic example-topic --from-beginning
	
Test Producer:

	docker exec --interactive --tty broker kafka-console-producer --bootstrap-server localhost:9092 --topic example-topic
