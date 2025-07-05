Vagrant.configure("2") do |config|
	
	config.vm.box = "bento/debian-12"
	config.vm.hostname = "taskmaster"

	config.vm.provision "shell", inline: <<-SHELL
		apt-get update
		apt-get install -y build-essential supervisor
	SHELL

	config.vm.synced_folder ".", "/home/vagrant/taskmaster"

  end
