
require 'mqtt/mqtt_hash.rb'

require_relative 'base_player.rb'

module LZRTag
	module Player
		class Hardware < Base
			attr_reader :team
			attr_reader :brightness

			attr_reader :dead, :deathChangeTime

			attr_reader :ammo
			attr_reader :maxAmmo
			attr_reader :gunNo

			attr_reader :position
			attr_reader :zoneIDs

			attr_reader :battery, :ping, :heap

			attr_reader :fireConfig, :hitConfig

			def initialize(*data)
				super(*data);

				@team = 0;
				@brightness = 0;

				@dead = false;
				@deathChangeTime = Time.now();

				@ammo = 0;
				@maxAmmo = 0;
				@gunNo = 0;

				@position = {x: 0, y: 0}
				@zoneIDs  = Hash.new();

				@battery = 0; @ping = 0; @heap = 0;

				# These values are configured for a DPS ~1, equal to all weapons
				# Including reload timings and other penalties
				@GunDamageMultipliers = [
					0.9138,
					1.85,
					0.6166,
				];

				@fireConfig = MQTT::TXHash.new(@mqtt, "Lasertag/Players/#{@DeviceID}/FireConf")
				@hitConfig  = MQTT::TXHash.new(@mqtt, "Lasertag/Players/#{@DeviceID}/HitConf")
			end

			def on_mqtt_data(data, topic)
				case topic[1..topic.length].join("/")
				when "System"
					if(data.size == 3*4)
						parsedData = data.unpack("L<*");

						@battery = parsedData[0].to_f/1000;
						@ping 	= parsedData[2].to_f;
					end
				when "Dead"
					dead = (data == "1")
					return if @dead == dead;
					@dead = dead;

					@deathChangeTime = Time.now();

					@handler.send_event(@dead ? :playerKilled : :playerRevived, self);
				when "Ammo"
					return if(data.size != 8)

					outData = data.unpack("L<*");
					@ammo = outData[0];
					@maxAmmo = outData[1];
				when "Position"
					begin
						@position = JSON.parse(data, symbolize_names: true);
					rescue
					end
				when "ZoneUpdate"
					begin
						data = JSON.parse(data, symbolize_names: true);
					rescue JSON::ParserError
						return;
					end

					@zoneIDs = data[:data];
					if(data[:entered])
						@handler.send_event(:playerEnteredZone, self, data[:entered])
					end
					if(data[:exited])
						@handler.send_event(:playerExitedZone, self, data[:exited])
					end
				else
					super(data, topic);
				end
			end

			def team=(n)
				n = n.to_i;
				raise ArgumentError, "Team out of range (must be between 0 and 255)" unless n <= 255 and n >= 0;

				return if @team == n;
				oldT = @team;
				@team = n;

				_pub_to "Team", @team, retain: true;
				@handler.send_event :playerTeamChanged, self, oldT;
				@team;
			end
			def brightness=(n)
				n = n.to_i;
				raise ArgumentError, "Brightness out of range (must be between 0 and 7)" unless n <= 7 and n >= 0;
				return if @brightness == n;

				@brightness = n;
				_pub_to "FX/Brightness", @brightness, retain: true;

				@brightness;
			end

			def _set_dead(d, player = nil)
				dead = (d ? true : false);
				return if @dead == dead;
				@dead = dead;

				@deathChangeTime = Time.now();

				_pub_to "Dead", @dead ? "1" : "0", retain: true;
				@handler.send_event(@dead ? :playerKilled : :playerRevived, self, player);
			end
			def dead=(d)
				_set_dead(d);
			end
			def kill_by(player)
				return if @dead;
				_set_dead(true, player);
			end

			def ammo=(n)
				unless (n.is_a?(Integer) and (n >= 0))
					raise ArgumentError, "Ammo amount needs to be a positive number!"
				end

				@ammo = n;

				_pub_to("Ammo/Set", n);
			end

			def gunNo=(n)
				unless (n.is_a?(Integer) and (n >= 0))
					raise ArgumentError, "Gun ID needs to be a positive integer!"
				end

				return if(@gunNo == n)

				@gunNo = n;
				_pub_to("GunNo", n, retain: true);
			end

			def gunDamage(number = nil)
				number = @gunNo if(number.nil?)

				return @GunDamageMultipliers[number-1] || 1;
			end

			def fireConfig=(h)
				@fireConfig.hash = h;
			end
			def hitConfig=(h)
				@hitConfig.hash = h;
			end

			def clear_all_topics()
				super();

				[	"Dead", "GunNo", "FX/Brightness", "Team",
					"HitConf", "FireConf"].each do |t|
					_pub_to(t, "", retain: true);
				end
			end
		end
	end
end
